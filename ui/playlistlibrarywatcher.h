#ifndef PLAYLISTLIBRARYWATCHER_H
#define PLAYLISTLIBRARYWATCHER_H

#include "library/librarywatcher.h"

class PlaylistTab;

class PlaylistLibraryWatcher : public LibraryWatcher
{
public:
	PlaylistLibraryWatcher(PlaylistTab *playlistTab);

public slots:
	void addTracks(const QList<std::shared_ptr<Track> > &tracks);
    void libraryChanged(LibraryEvent event);
	
private:
	PlaylistTab *playlistTab_;
};

#endif // PLAYLISTLIBRARYWATCHER_H
