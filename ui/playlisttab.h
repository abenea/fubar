#pragma once

#include <QList>
#include <QStringList>
#include <QWidget>
#include <memory>

#include "ui/playlistfilter.h"
#include "ui/playlistmodel_forward.h"

class PlaylistView;
class QDragEnterEvent;

namespace Ui {
class PlaylistFrame;
}

class PlaylistTab : public QWidget {
    Q_OBJECT
public:
    PlaylistTab(PModel model, QWidget *parent = 0);
    virtual ~PlaylistTab();

    // Can the user edit it?
    bool isEditable() const;

    void updateCursor(QModelIndex index);
    void updateCursorAndScroll(QModelIndex index);
    void focusFilter();

    void addUrls(const QList<QUrl> &urls);
    void addTracks(const QList<PTrack> &tracks);
    void removeTracks(QModelIndexList trackList);
    void serialize(QByteArray &data) const;
    void deserialize(const QByteArray &data);
    bool dropMimeData(const QMimeData *data);

    QModelIndex getRandomFilteredIndex();
    QModelIndex getFilteredIndex(QModelIndex current, int offset);
    QModelIndex getCurrentIndex();

    // Returns the position of index in the sorted, unfiltered playlist
    int getUnfilteredPosition(QModelIndex index);
    QModelIndex getUnfilteredPosition(int pos);

    // Back from the grave. Oh, the horrors...
    QModelIndex mapToSource(QModelIndex index) const;
    QModelIndexList mapToSource(QModelIndexList indexes) const;

protected slots:
    void changedFilter(const QString &filter);
    void doubleClicked(const QModelIndex &filterIndex);
    void clearFilterAndPlay();
    void repaintTracks(std::vector<QPersistentModelIndex> indexes);

private:
    std::unique_ptr<Ui::PlaylistFrame> ui_;
    PModel model_;
    PlaylistFilter filterModel_;

    friend class PlaylistView;
};
