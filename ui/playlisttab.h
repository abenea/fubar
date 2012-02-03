#ifndef PLAYLISTTAB_H
#define PLAYLISTTAB_H

#include "playlistmodel.h"
#include "playlistfilter.h"
#include <ui/ui_playlist.h>
#include "library/library.h"
#include <QWidget>
#include <QList>

struct LibraryEvent;
class Library;

// TODO: There should be a subclass of PlaylistTab called ViewTab or AutoSyncedPlaylistTab or smth
// that is non-editable and has the updateView based on library chages through signals and ***REMOVED***,
// and another class that is editable and doesnt care about library (the classic playlist).
class PlaylistTab : public QWidget
{
    Q_OBJECT
public:
    PlaylistTab(QWidget* parent = 0);

public slots:
    void play(const QModelIndex &index);
    void addDirectory(const QString &directory);

    void updateView(LibraryEvent event);
    // TODO: This code is probably bad/retarded, need to learn a bit more about qt to do it
    // But I'm too lazy atm
    void yunorefresh();

private:
    Ui::PlaylistFrame ui_;
    Playlist playlist_;
    PlaylistModel model_;
    PlaylistFilter filterModel_;

private slots:
    void changedFilter(const QString &filter);

    friend class Library;
};

#endif // PLAYLISTTAB_H
