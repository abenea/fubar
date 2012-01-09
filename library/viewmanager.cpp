#include "viewmanager.h"

#include "ui/playlisttab.h"
#include <QDebug>
#include <QTimer>
#include <QMutexLocker>
#include <boost/foreach.hpp>

void ViewManager::run()
{
     QTimer* timer = new QTimer();
     connect(timer, SIGNAL(timeout()), this, SLOT(update()));
     timer->start(1000);
     exec();
}

void ViewManager::quit()
{
    exit(0);
}

void ViewManager::updateViews(LibraryEvent event)
{
    QMutexLocker locker(&mutex_);
    qDebug() << (event.op2str().c_str()) << " " << event.track->location;
    events_.append(event);
}

void ViewManager::update()
{
    QMutexLocker locker(&mutex_);
    QList<LibraryEvent> events(events_);
    events_.clear();
    emit libraryUpdated(events);
}

PlaylistTab* ViewManager::createView()
{
    PlaylistTab* view = new PlaylistTab(&mainwindow_);
    QMutexLocker locker(&mutex_);
    QObject::connect(this, SIGNAL(libraryUpdated(QList<LibraryEvent>)), view, SLOT(updateView(QList<LibraryEvent>)));
    library_.getPlaylist(view->playlist_);
    views_.append(view);
    foreach (boost::shared_ptr<Track> track, view->playlist_.tracks) {
        qDebug() << track->location;
    }
    view->yunorefresh();
    return view;
}


#include "viewmanager.moc"