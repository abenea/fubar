#include "libraryviewplaylist.h"
#include "library/library.h"

LibraryViewPlaylist::LibraryViewPlaylist(QWidget* parent) : PlaylistTab(parent)
{
}

void LibraryViewPlaylist::updateView(LibraryEvent event)
{
    model_.updateView(event);
}

void LibraryViewPlaylist::addDirectory(const QString& directory)
{
    // Playlist cannot be edited
}

void LibraryViewPlaylist::addFiles(const QStringList& files)
{
    // Playlist cannot be edited
}

#include "libraryviewplaylist.moc"