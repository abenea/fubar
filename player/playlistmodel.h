#pragma once
#include "player/playlist.h"

#include <QAbstractTableModel>
#include <QStringList>
#include <memory>
#include <vector>

class Library;
class LibraryEvent;

class PlaylistModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    PlaylistModel(Library* library, QObject *parent = 0);

    virtual int rowCount(const QModelIndex &) const;
    virtual int columnCount(const QModelIndex &) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    Playlist &playlist() { return playlist_; }
    void addDirectory(const QString &path);
    void addFiles(const QStringList& files);
    void addTracks(QList<std::shared_ptr<Track>> tracks);
    void removeIndexes(QModelIndexList indexes);
    void clear();

    void notifyQueueStatusChanged(std::vector<QPersistentModelIndex> indexes);

signals:
    void queueStatusChanged(std::vector<QPersistentModelIndex> indexes);

protected slots:
    void libraryChanged(LibraryEvent event);
    void libraryChanged(QList<std::shared_ptr<Track>> tracks);

private:
    Playlist playlist_;
};

enum PlaylistRoles {
    TrackRole = Qt::UserRole,
    GroupingModeRole,
    GroupItemsRole
};

Q_DECLARE_METATYPE(PTrack)
