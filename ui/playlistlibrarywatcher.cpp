#include "playlistlibrarywatcher.h"
#include "playlisttab.h"
#include <library/library.h>

using namespace std;

PlaylistLibraryWatcher::PlaylistLibraryWatcher(PlaylistTab* playlistTab)
	: playlistTab_(playlistTab)
{
}

void PlaylistLibraryWatcher::addTracks(const QList< shared_ptr< Track > >& tracks)
{
    playlistTab_->addTracks(tracks);
}

void PlaylistLibraryWatcher::libraryChanged(LibraryEvent event)
{
	playlistTab_->libraryChanged(event);
}
