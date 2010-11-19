#include "playlistmodel.h"
#include <string>

using namespace boost;
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

QModelIndex PlaylistModel::parent(const QModelIndex& index) const
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

#include "playlistmodel.moc"