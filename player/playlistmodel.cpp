#include "playlistmodel.h"
#include "library/library.h"
#include "library/track.h"
#include <string>
#include <QDebug>
#include <cassert>

using namespace std;

PlaylistModel::PlaylistModel(Library* library, QObject *parent)
    : QAbstractItemModel(parent)
{
    if (library) {
        playlist_.synced = true;
        addTracks(library->getTracks());
        QObject::connect(library, SIGNAL(libraryChanged(LibraryEvent)), this, SLOT(libraryChanged(LibraryEvent)));
        QObject::connect(library, SIGNAL(libraryChanged(QList<PTrack>)), this, SLOT(libraryChanged(QList<PTrack>)));
    } else
        playlist_.synced = false;
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
    if (!playlist_.synced)
        return;

    if (event.op == CREATE) {
        beginInsertRows(QModelIndex(), playlist_.tracks.size(), playlist_.tracks.size());
        playlist_.tracks.append(event.track);
        qDebug() << "UPDATING MODEL: " << event.track->metadata["artist"] << " " << event.track->metadata["title"] <<
        " " << event.track->audioproperties.length;
        endInsertRows();
    } else if (event.op == MODIFY) {
        int i = 0;
        // could be using setData, but not sure it's worth it
        for (const auto& track : playlist_.tracks) {
            if (track->location == event.track->location) {
                playlist_.tracks.replace(i, event.track);
                emit dataChanged(index(i, 0, QModelIndex()), index(i, 0, QModelIndex()));
                break;
            }
            ++i;
        }
    } else if (event.op == DELETE) {
        int i = 0;
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
    } else if (event.op == DELETE_CUE) {
        int i = 0;
        for (QList<PTrack>::iterator it = playlist_.tracks.begin(); it != playlist_.tracks.end(); ++it) {
            PTrack track = *it;
            if (track->location == event.track->location && track->isCueTrack()) {
                beginRemoveRows(QModelIndex(), i, i);
                playlist_.tracks.erase(it);
                endRemoveRows();
            }
            ++i;
        }
    }
}

void PlaylistModel::libraryChanged(QList<PTrack> tracks)
{
    if (playlist_.synced) {
        clear();
        addTracks(tracks);
    }
}

void PlaylistModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, playlist_.tracks.size() - 1);
    playlist_.tracks.clear();
    endRemoveRows();
}

void PlaylistModel::addTracks(QList<PTrack> tracks)
{
    beginInsertRows(QModelIndex(), playlist_.tracks.size(), playlist_.tracks.size() + tracks.size() - 1);
    playlist_.tracks.append(tracks);
    endInsertRows();
}

void PlaylistModel::removeIndexes(QModelIndexList indexes)
{
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

void PlaylistModel::notifyQueueStatusChanged(vector<QPersistentModelIndex> indexes)
{
    emit queueStatusChanged(indexes);
}

#include "playlistmodel.moc"