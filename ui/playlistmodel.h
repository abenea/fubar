#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>
#include <library/playlist.h>
#include <QStringList>

class LibraryEvent;

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
    void addFiles(const QStringList& files);
    void libraryChanged(LibraryEvent event);
    void addTracks(QList<std::shared_ptr<Track>> tracks);
    void removeIndexes(QModelIndexList indexes);
    void clear();

    QModelIndex getIndex(QString path);

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

Q_DECLARE_METATYPE(std::shared_ptr<Track>)
Q_DECLARE_METATYPE(Grouping::Mode)

#endif // PLAYLISTMODEL_H
