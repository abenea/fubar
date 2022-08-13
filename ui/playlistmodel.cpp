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
    for (const QUrl &url : toAdd) {
        if (!url.host().contains("youtube.com"))
            continue;
        QProcess *ytcue = new QProcess();
        QObject::connect(ytcue, SIGNAL(readyRead()), this, SLOT(youtubeCueOutput()));
        QObject::connect(ytcue, SIGNAL(finished(int)), this, SLOT(youtubeCueFinished(int)));
        QObject::connect(ytcue, SIGNAL(errorOccurred(QProcess::ProcessError)), this,
                         SLOT(youtubeCueError(QProcess::ProcessError)));
        ytcue->start("youtube-cue --musicbrainz-app fubar --musicbrainz-version 0.1 " +
                     url.toString());
    }
    for (const QUrl &url : youtubePlaylists) {
        QProcess *ytdl = new QProcess();
        youtubeDlBuffers[ytdl] = QString();
        QObject::connect(ytdl, SIGNAL(readyReadStandardOutput()), this, SLOT(youtubeDlOutput()));
        QObject::connect(ytdl, SIGNAL(finished(int)), this, SLOT(youtubeDlFinished(int)));
        QObject::connect(ytdl, SIGNAL(errorOccurred(QProcess::ProcessError)), this,
                         SLOT(youtubeDlError(QProcess::ProcessError)));
        ytdl->start("youtube-dl -j --ignore-errors " + url.toString());
    }
}

void PlaylistModel::processError(QProcess::ProcessError error, QString processName) {
    QProcess *process = qobject_cast<QProcess *>(QObject::sender());
    qWarning() << processName << " process error " << error;
    process->deleteLater();
}

void PlaylistModel::processFinished(int status, QString processName) {
    QProcess *process = qobject_cast<QProcess *>(QObject::sender());
    if (status)
        qWarning() << processName << " returned status " << status;
    process->deleteLater();
}

void PlaylistModel::youtubeCueFinished(int status) { processFinished(status, "youtube-cue"); }

void PlaylistModel::youtubeCueError(QProcess::ProcessError error) {
    processError(error, "youtube-cue");
}

void PlaylistModel::youtubeCueOutput() {
    QProcess *ytcue = qobject_cast<QProcess *>(QObject::sender());
    QByteArray output = ytcue->readAll();
    qDebug() << "read from yt-cue\n" << QString(output);
    QJsonParseError parseError;
    auto jsonDoc = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Error parsing json from youtube-cue at offset " << parseError.offset << ": "
                   << parseError.errorString();
        return;
    }
    if (!jsonDoc.isObject() || !jsonDoc.object().contains("tracks") ||
        !jsonDoc.object().value("tracks").isArray() ||
        jsonDoc.object().value("tracks").toArray().size() == 0)
        return;

    auto o = jsonDoc.object().toVariantMap();
    QString url = o["url"].toString();
    // Remove url
    int i = 0;
    for (QList<PTrack>::iterator it = playlist_.tracks.begin(); it != playlist_.tracks.end();
         ++it) {
        PTrack &track = *it;
        if (track->location == url && !track->isCueTrack()) {
            beginRemoveRows(QModelIndex(), i, i);
            it = playlist_.tracks.erase(it);
            endRemoveRows();
            break;
        }
        ++i;
    }

    // Add url cue tracks
    int oldSize = playlist_.tracks.size();
    i = 0;
    for (const auto &info : jsonDoc.object().value("tracks").toArray()) {
        QVariantMap track_info = info.toObject().toVariantMap();
        shared_ptr<Track> track(new Track());
        track->location = url;
        if (o.count("artist"))
            track->metadata["artist"] = o["artist"].toString();
        track->metadata["album"] = o.count("album") ? o["album"].toString() : o["title"].toString();
        track->metadata["title"] = track_info["title"].toString();
        track->metadata["_cue_offset"] = QString::number(track_info["offset"].toInt() * 1000);
        track->metadata["track"] = QString::number(++i);
        track->audioproperties.length = track_info["duration"].toInt();
        playlist_.tracks.append(track);
    }
    int newSize = playlist_.tracks.size();
    if (newSize > oldSize) {
        beginInsertRows(QModelIndex(), oldSize, newSize - 1);
        endInsertRows();
    }
}

void PlaylistModel::youtubeDlError(QProcess::ProcessError error) {
    QProcess *process = qobject_cast<QProcess *>(QObject::sender());
    youtubeDlBuffers.erase(process);
    processError(error, "youtube-dl");
}

void PlaylistModel::youtubeDlFinished(int status) {
    QProcess *process = qobject_cast<QProcess *>(QObject::sender());
    youtubeDlBuffers.erase(process);
    processFinished(status, "youtube-dl");
}

void PlaylistModel::youtubeDlOutput() {
    QProcess *process = qobject_cast<QProcess *>(QObject::sender());
    QTextStream stdoutStream(process->readAllStandardOutput());
    int oldSize = playlist_.tracks.size();

    while (true) {
        QString line = stdoutStream.readLine();
        if (line.isNull())
            break;
        if (!youtubeDlBuffers[process].isEmpty()) {
            line = youtubeDlBuffers[process] + line;
            youtubeDlBuffers[process].clear();
        }

        QJsonParseError parseError;
        auto jsonDoc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "Error parsing json from youtube-dl at offset " << parseError.offset
                       << ": " << parseError.errorString();
            if (stdoutStream.atEnd())
                youtubeDlBuffers[process] = line;
            continue;
        }
        if (!jsonDoc.isObject() || !jsonDoc.object().contains("webpage_url"))
            continue;

        QVariantMap track_info = jsonDoc.object().toVariantMap();
        shared_ptr<Track> track(new Track());
        track->location = track_info["webpage_url"].toString();
        if (track_info.count("artist"))
            track->metadata["artist"] = track_info["artist"].toString();
        if (track_info.count("album"))
            track->metadata["album"] = track_info["album"].toString();
        if (track_info.count("track"))
            track->metadata["title"] = track_info["track"].toString();
        track->audioproperties.length = track_info["duration"].toInt();
        playlist_.tracks.append(track);
    }

    int newSize = playlist_.tracks.size();
    if (newSize > oldSize) {
        beginInsertRows(QModelIndex(), oldSize, newSize - 1);
        endInsertRows();
    }
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
