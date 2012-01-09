#ifndef PLAYLISTTAB_H
#define PLAYLISTTAB_H

#include "playlistmodel.h"
#include "playlistfilter.h"
#include <ui/ui_playlist.h>
#include "library/library.h"
#include <QWidget>
#include <QList>

struct LibraryEvent;
struct ViewManager;

class PlaylistTab : public QWidget
{
    Q_OBJECT
public:
    PlaylistTab(QWidget* parent = 0);

public slots:
    void play(const QModelIndex &index);
    void addDirectory(const QString &directory);
    // should be a separate class?

    void updateView(QList<LibraryEvent> events);
    // lol QNap
    void yunorefresh();

private:
    Ui::PlaylistFrame ui_;
    Playlist playlist_;
    PlaylistModel model_;
    PlaylistFilter filterModel_;

private slots:
    void changedFilter(const QString &filter);

    friend class ViewManager;
};

#endif // PLAYLISTTAB_H
