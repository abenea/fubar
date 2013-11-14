#pragma once

#include "player/playlistmodel_forward.h"
#include "playlistfilter.h"
#include "ui/ui_playlist.h"
#include <QWidget>
#include <QStringList>
#include <QList>
#include <memory>

class PlaylistView;

class PlaylistTab : public QWidget
{
    Q_OBJECT
public:
    PlaylistTab(PModel model, QWidget* parent = 0);

    // Can the user edit it?
    bool isEditable() const;

    void updateCursor(QModelIndex index);
    void updateCursorAndScroll(QModelIndex index);
    void focusFilter();

    void addDirectory(const QString &directory);
    void addFiles(const QStringList &files);
    void addTracks(const QList<PTrack> &tracks);
    void removeTracks(QModelIndexList trackList);
    void serialize(QByteArray& data) const;
    void deserialize(const QByteArray& data);

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
    Ui::PlaylistFrame ui_;
    PModel model_;
    PlaylistFilter filterModel_;

    friend class PlaylistView;
};
