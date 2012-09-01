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

public slots:
    void play(const QModelIndex &index);
    void play();
    void playNext(int offset);
    void enqueueNextTrack();
    void updateCurrentIndex();
    void updateCursor();
    void updateCursorAndScroll();
    virtual void addDirectory(const QString &directory);
    virtual void addFiles(const QStringList &files);
    virtual void addTracks(const QList<std::shared_ptr<Track>> &tracks);
    virtual void libraryChanged(LibraryEvent event);
    virtual void libraryChanged(QList<std::shared_ptr<Track>> tracks);

    // TODO: This code is probably bad/retarded, need to learn a bit more about qt to do it
    // But I'm too lazy atm
    void yunorefresh();

    PTrack getCurrentTrack();

protected:
    bool synced_;
    Ui::PlaylistFrame ui_;
    Playlist playlist_;
    PlaylistModel model_;
    PlaylistFilter filterModel_;
    QPersistentModelIndex current_index_;
    QPersistentModelIndex next_index_;

protected slots:
    void changedFilter(const QString &filter);
};

#endif // PLAYLISTTAB_H
