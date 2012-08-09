#ifndef LIBRARYWATCHER_H
#define LIBRARYWATCHER_H

#include "track.h"
#include <QObject>
#include <memory>

class LibraryEvent;

class LibraryWatcher : public QObject
{
public:
	virtual ~LibraryWatcher() {}
	
public slots:
	virtual void addTracks(const QList<std::shared_ptr<Track> > &tracks) = 0;
    virtual void libraryChanged(LibraryEvent event) = 0;
};

#endif // LIBRARYWATCHER_H
