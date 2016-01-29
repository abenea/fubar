#include "playlistview.h"
#include "mainwindow.h"
#include "playlistitemdelegate.h"
#include "playlisttab.h"
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
        PlaylistTab* playlistTab = MainWindow::instance->getActivePlaylist();
        if (playlistTab) {
            MainWindow::instance->enqueueTracks(playlistTab->model_,
                                                playlistTab->filterModel_.mapSelectionToSource(selectionModel()->selection()).indexes());
        }
    } else if (event->key() == Qt::Key_Delete) {
        PlaylistTab* playlistTab = MainWindow::instance->getActivePlaylist();
        if (playlistTab) {
            playlistTab->removeTracks(selectedIndexes());
        }
    } else {
        QAbstractItemView::keyPressEvent(event);
    }
}
