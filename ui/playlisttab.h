#ifndef PLAYLISTTAB_H
#define PLAYLISTTAB_H

#include "playlistmodel.h"
#include "playlistfilter.h"
#include "ui/ui_playlist.h"
#include <QWidget>
#include <QStringList>
#include <QList>

class PlaylistTab : public QWidget
{
    Q_OBJECT
public:
    PlaylistTab(bool synced, QWidget* parent = 0);

    // so uncool having this here
    QModelIndexList mapToSource(QModelIndexList indexes) const;

    PTrack getCurrentTrack();

    // Can the user edit it?
    bool isEditable() { return !synced_; }

    void play(const QModelIndex &index);
    void play();
    void playNext(int offset);

    void enqueueNextTrack();
    void enqueueTrack(QModelIndex index);

    void currentSourceChanged();

    void updateCursorAndScroll();
    void repaintTrack(const QModelIndex& index);
    void focusFilter();

    void addDirectory(const QString &directory);
    void addFiles(const QStringList &files);
    void addTracks(const QList<std::shared_ptr<Track>> &tracks);
    void removeTracks(QModelIndexList trackList);

public slots:
    void libraryChanged(LibraryEvent event);
    void libraryChanged(QList<std::shared_ptr<Track>> tracks);

protected slots:
    void updateCursor();
    void changedFilter(const QString &filter);
    void doubleClicked(const QModelIndex &filterIndex);
    void clearFilterAndPlay();

protected:
    QModelIndex getNextModelIndex(int offset);

protected:
    bool synced_;
    Ui::PlaylistFrame ui_;
    Playlist playlist_;
    PlaylistModel model_;
    PlaylistFilter filterModel_;
    QPersistentModelIndex currentIndex_;
    QPersistentModelIndex nextIndex_;

private:
    // Used for testing
    void removeTrackAt(int position);
    void enqueuePosition(int pos);

    friend class PlayerTest;
};

#endif // PLAYLISTTAB_H
