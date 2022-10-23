#pragma once

#include "player/jsonprocess.h"
#include "player/playlist.h"

#include <QAbstractTableModel>
#include <QJsonDocument>
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

    int rowCount(const QModelIndex &) const override;
    int columnCount(const QModelIndex &) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex
    index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data,
                      Qt::DropAction action,
                      int row,
                      int column,
                      const QModelIndex &parent) override;

    Playlist &playlist() { return playlist_; }
    void addUrls(const QList<QUrl> &urls);
    void addTracks(QList<PTrack> tracks);
    void removeIndexes(QModelIndexList indexes);
    void clear();
    void deserialize(const QByteArray &data);
    QList<PTrack> getTracks(QModelIndexList trackList) const;

    void notifyQueueStatusChanged(std::vector<QPersistentModelIndex> indexes);

    void setFakeYoutubeDl(QString output) { fakeYoutubeDl_ = output; }

public slots:
    void libraryChanged(LibraryEvent event);

signals:
    void queueStatusChanged(std::vector<QPersistentModelIndex> indexes);
    void youtubeDlDone();

protected slots:
    void libraryChanged(QList<PTrack> tracks);
    void youtubeDocument(const QJsonDocument &doc);
    void youtubeDlFinished(int status);
    void youtubeDlError(QProcess::ProcessError error);
    void youtubeDlParseError(QJsonParseError error);

private:
    void finishYoutubeDl(JsonProcess *process);

    Playlist playlist_;
    Qt::DropActions dropActions_;
    QString fakeYoutubeDl_;
};

enum PlaylistRoles { TrackRole = Qt::UserRole, GroupingModeRole, GroupItemsRole };

Q_DECLARE_METATYPE(PTrack)
