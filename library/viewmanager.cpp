#include "viewmanager.h"

#include "ui/libraryviewplaylist.h"
#include <QDebug>
#include <QMutexLocker>

LibraryViewPlaylist* ViewManager::createView()
{
    LibraryViewPlaylist* view = new LibraryViewPlaylist(&mainwindow_);
    PlaylistTab* tab = new PlaylistTab(&mainwindow_);
    library_.registerView(view);
    views_.append(view);
    view->yunorefresh();
    return view;
}
