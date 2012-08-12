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
    PlaylistTab(QWidget* parent = 0);

public slots:
    void play(const QModelIndex &index);
    void play();
    void playNext(QString path, int offset);
    void enqueueNextTrack();
    virtual void addDirectory(const QString &directory);
    virtual void addFiles(const QStringList &files);
    virtual void addTracks(const QList<std::shared_ptr<Track> > &tracks);
    virtual void libraryChanged(LibraryEvent event);

    // TODO: This code is probably bad/retarded, need to learn a bit more about qt to do it
    // But I'm too lazy atm
    void yunorefresh();

protected:
    Ui::PlaylistFrame ui_;
    Playlist playlist_;
    PlaylistModel model_;
    PlaylistFilter filterModel_;

protected slots:
    void changedFilter(const QString &filter);
};

#endif // PLAYLISTTAB_H
