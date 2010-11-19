#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>
#include <library/playlist.h>
#include <QStringList>

class PlaylistModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    PlaylistModel(Playlist &playlist, QObject *parent = 0);

    virtual int rowCount(const QModelIndex &) const;
    virtual int columnCount(const QModelIndex &) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    Playlist &playlist() { return playlist_; }
    void addDirectory(const QString &path);
private:
    Playlist &playlist_;
};

enum PlaylistRoles {
    TrackRole = Qt::UserRole,
    GroupingModeRole,
    GroupItemsRole
};

namespace Grouping
{
    enum Mode
    {
        None = 1,
        Head,
        Body,
        Tail,
        Invalid
    };
}

Q_DECLARE_METATYPE(boost::shared_ptr<Track>)
Q_DECLARE_METATYPE(Grouping::Mode)

#endif // PLAYLISTMODEL_H
