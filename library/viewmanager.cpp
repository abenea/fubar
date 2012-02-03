#include "viewmanager.h"

#include "ui/playlisttab.h"
#include <QDebug>
#include <QMutexLocker>

PlaylistTab* ViewManager::createView()
{
    PlaylistTab* view = new PlaylistTab(&mainwindow_);
    library_.registerView(view);
    views_.append(view);
    view->yunorefresh();
    return view;
}
