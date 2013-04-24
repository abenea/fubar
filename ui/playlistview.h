#pragma once

#include <QListView>
//#include <QMenu>

class PlaylistItemDelegate;

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
