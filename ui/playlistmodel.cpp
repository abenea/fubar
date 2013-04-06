#include "playlistmodel.h"
#include "library/library.h"
#include "library/track.h"
#include <string>
#include <QDebug>

using namespace std;

PlaylistModel::PlaylistModel(Playlist &playlist, QObject *parent)
    : QAbstractItemModel(parent), playlist_(playlist)
{
}

int PlaylistModel::rowCount(const QModelIndex &) const
{
    return playlist_.tracks.size();
}

int PlaylistModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
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

bool PlaylistModel::hasChildren(const QModelIndex& parent) const
{
    if (parent.isValid())
        return false;
    else
        return true;
}

QModelIndex PlaylistModel::parent(const QModelIndex& /*index*/) const
{
    return QModelIndex();
}

QModelIndex PlaylistModel::index(int row, int column, const QModelIndex &parent) const
{
    assert(!parent.isValid());
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    return createIndex(row, column);
}

void PlaylistModel::addDirectory(const QString& path)
{
    int oldSize = playlist_.tracks.size();
    playlist_.addDirectory(path);
    int newSize = playlist_.tracks.size();
    if (newSize > oldSize) {
        beginInsertRows(QModelIndex(), oldSize, newSize - 1);
        endInsertRows();
    }
}

void PlaylistModel::addFiles(const QStringList& files)
{
    int oldSize = playlist_.tracks.size();
    playlist_.addFiles(files);
    int newSize = playlist_.tracks.size();
    if (newSize > oldSize) {
        beginInsertRows(QModelIndex(), oldSize, newSize - 1);
        endInsertRows();
    }
}

void PlaylistModel::libraryChanged(LibraryEvent event)
{
    if (event.op == CREATE) {
        beginInsertRows(QModelIndex(), playlist_.tracks.size(), playlist_.tracks.size());
        playlist_.tracks.append(event.track);
        qDebug() << "UPDATING MODEL: " << event.track->metadata["artist"] << " " << event.track->metadata["title"] <<
        " " << event.track->audioproperties.length;
        endInsertRows();
    } else if (event.op == MODIFY) {
        int i = 0;
        // todo should probably use setData()
        foreach (PTrack track, playlist_.tracks) {
            if (track->location == event.track->location) {
                playlist_.tracks.replace(i, event.track);
                emit dataChanged(index(i, 0, QModelIndex()), index(i, 0, QModelIndex()));
                break;
            }
            ++i;
        }
    } else if (event.op == DELETE) {
        int i = 0;
        // todo should probably use setData()
        for (QList<PTrack>::iterator it = playlist_.tracks.begin(); it != playlist_.tracks.end(); ++it) {
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

void PlaylistModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, playlist_.tracks.size() - 1);
    playlist_.tracks.clear();
    endRemoveRows();
}

void PlaylistModel::addTracks(QList<std::shared_ptr<Track>> tracks)
{
    beginInsertRows(QModelIndex(), playlist_.tracks.size(), playlist_.tracks.size() + tracks.size() - 1);
    playlist_.tracks.append(tracks);
    endInsertRows();
}

void PlaylistModel::removeTracks(QModelIndexList tracks)
{
    for (auto track : tracks) {
        beginRemoveRows(QModelIndex(), track.row(), track.row());
        playlist_.tracks.removeAt(track.row());
        endRemoveRows();
    }
}

QModelIndex PlaylistModel::getIndex(QString path)
{
    int i = 0;
    for (QList<PTrack>::iterator it = playlist_.tracks.begin(); it != playlist_.tracks.end(); ++it) {
        PTrack track = *it;
        if (track->location == path) {
            return index(i, 0);
        }
        ++i;
    }
    return QModelIndex();
}

#include "playlistmodel.moc"