#ifndef VIEWMANAGER_H
#define VIEWMANAGER_H

#include "track.h"
#include "library.h"
#include "ui/mainwindow.h"
#include <QMutex>
#include <QQueue>
#include <QList>

struct LibraryEvent;
class LibraryViewPlaylist;
class Library;
class MainWindow;

class ViewManager
{
public:
    ViewManager(MainWindow& mainwindow, Library& library) : mainwindow_(mainwindow), library_(library) {}
    LibraryViewPlaylist* createView();

private:
    MainWindow& mainwindow_;
    Library& library_;
    QList<LibraryViewPlaylist*> views_;
};

#endif // VIEWMANAGER_H
