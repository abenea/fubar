#include "ui/playlistmodel.h"
#include "library/library.h"
#include "library/track.h"
#include "player/playlistmimedata.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QUrl>
#include <cassert>
#include <sstream>
#include <string>

using namespace std;

PlaylistModel::PlaylistModel(bool editable, QObject *parent) : QAbstractItemModel(parent) {
    dropActions_ = editable ? Qt::CopyAction : Qt::CopyAction | Qt::MoveAction;
    playlist_.synced = !editable;
}

int PlaylistModel::rowCount(const QModelIndex &) const { return playlist_.tracks.size(); }

int PlaylistModel::columnCount(const QModelIndex &) const { return 1; }

QVariant PlaylistModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    if (role == TrackRole) {
        QVariant ret;
        ret.setValue(playlist_.tracks[index.row()]);
        return ret;
    } else {
        return QVariant();
    }
}

QStringList PlaylistModel::mimeTypes() const {
    QStringList types;
    types << "text/uri-list";
    types << "binary/playlist";
    return types;
}

Qt::DropActions PlaylistModel::supportedDropActions() const { return dropActions_; }

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

    if (index.isValid()) {
        return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
    } else {
        return Qt::ItemIsDropEnabled | defaultFlags;
    }
}

QMimeData *PlaylistModel::mimeData(const QModelIndexList &indexes) const {
    return new PlaylistMimeData(getTracks(indexes));
}

bool PlaylistModel::dropMimeData(
    const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    if (action == Qt::IgnoreAction) {
        return true;
    }
    if (!data->hasUrls() && !data->formats().contains("binary/playlist")) {
        return false;
    }

    const PlaylistMimeData *myData = qobject_cast<const PlaylistMimeData *>(data);
    if (myData) {
        addTracks(myData->getTracks());
        return true;
    } else if (data->hasUrls()) {
        addUrls(data->urls());
        return true;
    }
    return false;
}

bool PlaylistModel::hasChildren(const QModelIndex &parent) const {
    if (parent.isValid())
        return false;
    else
        return true;
}

QModelIndex PlaylistModel::parent(const QModelIndex & /*index*/) const { return QModelIndex(); }

QModelIndex PlaylistModel::index(int row, int column, const QModelIndex &parent) const {
    assert(!parent.isValid());
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    return createIndex(row, column);
}

void PlaylistModel::addUrls(const QList<QUrl> &urls) {
    QList<QUrl> youtubePlaylists, toAdd;
    for (const QUrl &url : urls) {
        if (url.toString().contains("playlist?list="))
            youtubePlaylists.push_back(url);
        else
            toAdd.push_back(url);
    }
    int oldSize = playlist_.tracks.size();
    playlist_.addUrls(toAdd);
    int newSize = playlist_.tracks.size();
    if (newSize > oldSize) {
        beginInsertRows(QModelIndex(), oldSize, newSize - 1);
        endInsertRows();
    }
    for (const QUrl &url : youtubePlaylists) {
        JsonProcess *ytdl = new JsonProcess;
        if (fakeYoutubeDl_.isEmpty()) {
            ytdl->start("yt-dlp", QStringList{"-j", "--ignore-errors", url.toString()});
        } else {
            ytdl->start("echo", QStringList{fakeYoutubeDl_});
        }
        connect(ytdl, &JsonProcess::document, this, &PlaylistModel::youtubeDocument);
        connect(ytdl, &JsonProcess::parseError, this, &PlaylistModel::youtubeDlParseError);
        connect(ytdl, &JsonProcess::finished, this, &PlaylistModel::youtubeDlFinished);
        connect(ytdl, &JsonProcess::errorOccurred, this, &PlaylistModel::youtubeDlError);
    }
}

void PlaylistModel::youtubeDlParseError(QJsonParseError error) {
    qWarning() << "Error parsing json from youtube-dl at offset:" << error.offset << ":"
               << error.errorString();
}

void PlaylistModel::youtubeDlError(QProcess::ProcessError error) {
    qWarning() << "youtube-dl process error:" << error;
    JsonProcess *process = qobject_cast<JsonProcess *>(QObject::sender());
    finishYoutubeDl(process);
}

void PlaylistModel::youtubeDlFinished(int status) {
    JsonProcess *process = qobject_cast<JsonProcess *>(QObject::sender());
    if (status)
        qWarning() << "youtube-dl returned status:" << status;
    if (!process->stderr().isEmpty())
        qWarning() << process->stderr().constData();
    finishYoutubeDl(process);
}

void PlaylistModel::finishYoutubeDl(JsonProcess *process) {
    process->deleteLater();
    emit youtubeDlDone();
}

void PlaylistModel::youtubeDocument(const QJsonDocument &doc) {
    if (!doc.isObject() || !doc.object().contains("webpage_url"))
        return;

    QVariantMap track_info = doc.object().toVariantMap();
    shared_ptr<Track> track(new Track());
    track->location = track_info["webpage_url"].toString();
    if (track_info.count("artist"))
        track->metadata["artist"] = track_info["artist"].toString();
    if (track_info.count("album"))
        track->metadata["album"] = track_info["album"].toString();
    if (track_info.count("track"))
        track->metadata["title"] = track_info["track"].toString();
    if (track_info.count("playlist_index"))
        track->metadata["track"] = track_info["playlist_index"].toString();
    track->audioproperties.length = track_info["duration"].toInt();
    beginInsertRows(QModelIndex(), playlist_.tracks.size(), playlist_.tracks.size());
    playlist_.tracks.append(track);
    endInsertRows();
}

void PlaylistModel::deserialize(const QByteArray &data) {
    playlist_.deserialize(data);
    int newSize = playlist_.tracks.size();
    beginInsertRows(QModelIndex(), 0, newSize - 1);
    endInsertRows();
}

QList<PTrack> PlaylistModel::getTracks(QModelIndexList trackList) const {
    QList<PTrack> tracks;
    for (auto &index : trackList)
        tracks.append(playlist_.tracks.at(index.row()));
    return tracks;
}

void PlaylistModel::libraryChanged(LibraryEvent event) {
    if (event.op == CREATE) {
        beginInsertRows(QModelIndex(), playlist_.tracks.size(), playlist_.tracks.size());
        playlist_.tracks.append(event.track);
        qDebug() << "UPDATING MODEL: " << event.track->metadata["artist"] << " "
                 << event.track->metadata["title"] << " " << event.track->audioproperties.length;
        endInsertRows();
    } else if (event.op == MODIFY) {
        int i = 0;
        // could be using setData, but not sure it's worth it
        for (const auto &track : playlist_.tracks) {
            if (track->location == event.track->location) {
                playlist_.tracks.replace(i, event.track);
                emit dataChanged(index(i, 0, QModelIndex()), index(i, 0, QModelIndex()));
                break;
            }
            ++i;
        }
    } else if (event.op == DELETE) {
        int i = 0;
        for (QList<PTrack>::iterator it = playlist_.tracks.begin(); it != playlist_.tracks.end();
             ++it) {
            PTrack track = *it;
            if (track->location == event.track->location) {
                beginRemoveRows(QModelIndex(), i, i);
                playlist_.tracks.erase(it);
                endRemoveRows();
                break;
            }
            ++i;
        }
    }
}

void PlaylistModel::libraryChanged(QList<PTrack> tracks) {
    clear();
    addTracks(tracks);
}

void PlaylistModel::clear() {
    beginRemoveRows(QModelIndex(), 0, playlist_.tracks.size() - 1);
    playlist_.tracks.clear();
    endRemoveRows();
}

void PlaylistModel::addTracks(QList<PTrack> tracks) {
    beginInsertRows(QModelIndex(), playlist_.tracks.size(),
                    playlist_.tracks.size() + tracks.size() - 1);
    playlist_.tracks.append(tracks);
    endInsertRows();
}

void PlaylistModel::removeIndexes(QModelIndexList indexes) {
    std::vector<QPersistentModelIndex> pindexes;
    pindexes.reserve(indexes.size());
    for (auto index : indexes)
        pindexes.push_back(QPersistentModelIndex(index));
    for (auto index : pindexes) {
        beginRemoveRows(QModelIndex(), index.row(), index.row());
        playlist_.tracks.removeAt(index.row());
        endRemoveRows();
    }
}

void PlaylistModel::notifyQueueStatusChanged(vector<QPersistentModelIndex> indexes) {
    emit queueStatusChanged(indexes);
}
