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

public slots:
    void play(const QModelIndex &index);
    void play();
    void playNext(int offset);
    void enqueueNextTrack();
    void enqueueTrack(QModelIndex index);
    void updateCurrentIndex();
    void updateCursor();
    void updateCursorAndScroll();
    void repaintTrack(const QModelIndex& index);
    void focusFilter();
    virtual void addDirectory(const QString &directory);
    virtual void addFiles(const QStringList &files);
    virtual void addTracks(const QList<std::shared_ptr<Track>> &tracks);
    virtual void libraryChanged(LibraryEvent event);
    virtual void libraryChanged(QList<std::shared_ptr<Track>> tracks);

    PTrack getCurrentTrack();

protected slots:
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
};

#endif // PLAYLISTTAB_H
