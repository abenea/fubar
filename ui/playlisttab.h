#ifndef PLAYLISTTAB_H
#define PLAYLISTTAB_H

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
    PlaylistTab(std::shared_ptr<PlaylistModel> model, QWidget* parent = 0);

    // Can the user edit it?
    bool isEditable();

    void updateCursor(QModelIndex index);
    void updateCursorAndScroll(QModelIndex index);
    void focusFilter();

    void addDirectory(const QString &directory);
    void addFiles(const QStringList &files);
    void addTracks(const QList<std::shared_ptr<Track>> &tracks);
    void removeTracks(QModelIndexList trackList);

    QModelIndex getRandomFilteredIndex();
    QModelIndex getFilteredIndex(QModelIndex current, int offset);
    QModelIndex getCurrentIndex();

protected slots:
    void changedFilter(const QString &filter);
    void doubleClicked(const QModelIndex &filterIndex);
    void clearFilterAndPlay();
    void repaintTracks(std::vector<QPersistentModelIndex> indexes);

private:
    QModelIndexList mapToSource(QModelIndexList indexes) const;

    Ui::PlaylistFrame ui_;
    std::shared_ptr<PlaylistModel> model_;
    PlaylistFilter filterModel_;

    friend class PlaylistView;
};

#endif // PLAYLISTTAB_H
