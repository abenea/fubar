#pragma once

#include "player/playlist.h"

#include <QAbstractTableModel>
#include <QProcess>
#include <QStringList>
#include <map>
#include <memory>
#include <vector>

struct LibraryEvent;
class QMimeData;

class PlaylistModel : public QAbstractItemModel {
    Q_OBJECT
public:
    PlaylistModel(bool editable, QObject *parent = 0);

    virtual int rowCount(const QModelIndex &) const;
    virtual int columnCount(const QModelIndex &) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    Qt::DropActions supportedDropActions() const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data,
                      Qt::DropAction action,
                      int row,
                      int column,
                      const QModelIndex &parent);

    Playlist &playlist() { return playlist_; }
    void addUrls(const QList<QUrl> &urls);
    void addTracks(QList<PTrack> tracks);
    void removeIndexes(QModelIndexList indexes);
    void clear();
    void deserialize(const QByteArray &data);
    QList<PTrack> getTracks(QModelIndexList trackList) const;

    void notifyQueueStatusChanged(std::vector<QPersistentModelIndex> indexes);

public slots:
    void libraryChanged(LibraryEvent event);

signals:
    void queueStatusChanged(std::vector<QPersistentModelIndex> indexes);

protected slots:
    void libraryChanged(QList<PTrack> tracks);
    void youtubeCueOutput();
    void youtubeCueFinished(int status);
    void youtubeCueError(QProcess::ProcessError error);
    void youtubeDlOutput();
    void youtubeDlFinished(int status);
    void youtubeDlError(QProcess::ProcessError error);

private:
    void processFinished(int status, QString processName);
    void processError(QProcess::ProcessError error, QString processName);

    Playlist playlist_;
    Qt::DropActions dropActions_;
    std::map<QProcess *, QString> youtubeDlBuffers;
};

enum PlaylistRoles { TrackRole = Qt::UserRole, GroupingModeRole, GroupItemsRole };

Q_DECLARE_METATYPE(PTrack)
