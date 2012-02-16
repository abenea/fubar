#include "libraryviewplaylist.h"
#include "library/library.h"

LibraryViewPlaylist::LibraryViewPlaylist(QWidget* parent) : PlaylistTab(parent)
{
}

void LibraryViewPlaylist::updateView(LibraryEvent event)
{
    model_.updateView(event);
}

#include "libraryviewplaylist.moc"