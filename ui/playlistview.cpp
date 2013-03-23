#include "playlistview.h"
#include "mainwindow.h"
#include <QHeaderView>
#include <QAction>
#include <QKeyEvent>

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
    } else if (event->key() == Qt::Key_Q) {
        PlaylistTab* playlistTab = MainWindow::instance->getCurrentPlaylist();
        MainWindow::instance->queue.pushTracks(playlistTab, playlistTab->mapToSource(selectedIndexes()));
    } else {
        QAbstractItemView::keyPressEvent(event);
    }
}

#include "playlistview.moc"
