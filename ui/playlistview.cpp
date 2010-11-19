#include "playlistview.h"
#include <QHeaderView>
#include <QAction>

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

#include "playlistview.moc"
