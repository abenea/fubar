#include "playlistview.h"
#include <QHeaderView>
#include <QAction>
#include <QKeyEvent>
#include <QDebug>

PlaylistView::PlaylistView(QWidget *parent) : QListView(parent)
{
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    itemDelegate_ = new PlaylistItemDelegate(this);
    setItemDelegate(itemDelegate_);
}

// Overridden to disable keyboard search behavior
void PlaylistView::keyboardSearch(const QString &)
{
}

void PlaylistView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return) {
        emit returnPressed(currentIndex());
    } else {
        QAbstractItemView::keyPressEvent(event);
    }
}

#include "playlistview.moc"
