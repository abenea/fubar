#ifndef LIBRARYVIEWPLAYLIST_H
#define LIBRARYVIEWPLAYLIST_H

#include "ui/playlisttab.h"

struct LibraryEvent;
class Library;

class LibraryViewPlaylist : public PlaylistTab
{
    Q_OBJECT
public:
    LibraryViewPlaylist(QWidget* parent = 0);

public slots:
    void updateView(LibraryEvent event);

    friend class Library;
};

#endif // LIBRARYVIEWPLAYLIST_H