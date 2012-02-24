#include "viewmanager.h"

#include "ui/libraryviewplaylist.h"
#include <QDebug>
#include <QMutexLocker>

ViewManager *ViewManager::instance = 0;

ViewManager::ViewManager(MainWindow& mainwindow, Library& library)
    : mainwindow_(mainwindow), library_(library)
{
    instance = this;
}

LibraryViewPlaylist* ViewManager::createView()
{
    LibraryViewPlaylist* view = new LibraryViewPlaylist(&mainwindow_);
    library_.registerView(view);
    views_.append(view);
    view->yunorefresh();
    return view;
}
