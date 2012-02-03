#ifndef VIEWMANAGER_H
#define VIEWMANAGER_H

#include "track.h"
#include "library.h"
#include "ui/mainwindow.h"
#include <QMutex>
#include <QQueue>
#include <QList>

struct LibraryEvent;
class PlaylistTab;
class Library;
class MainWindow;

class ViewManager
{
public:
    ViewManager(MainWindow& mainwindow, Library& library) : mainwindow_(mainwindow), library_(library) {}
    PlaylistTab* createView();

private:
    MainWindow& mainwindow_;
    Library& library_;
    QList<PlaylistTab*> views_;
};

#endif // VIEWMANAGER_H
