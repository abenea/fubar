#ifndef PLAYLISTVIEW_H
#define PLAYLISTVIEW_H

#include <QListView>
#include <QMenu>
#include "playlistitemdelegate.h"

class PlaylistView : public QListView
{
    Q_OBJECT
public:
    PlaylistView(QWidget *parent = 0);

    void keyboardSearch(const QString &search);

signals:
    void returnPressed(const QModelIndex& index);

protected:
    void keyPressEvent(QKeyEvent* event);

private:
    PlaylistItemDelegate *itemDelegate_;
};

#endif // PLAYLISTVIEW_H
