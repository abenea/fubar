#ifndef VIEWMANAGER_H
#define VIEWMANAGER_H

#include "track.h"
#include "library.h"
#include "ui/mainwindow.h"
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QList>

struct LibraryEvent;
class PlaylistTab;
class Library;
class MainWindow;

class ViewManager : public QThread
{
    Q_OBJECT
public:
    ViewManager(MainWindow& mainwindow, Library& library) : mainwindow_(mainwindow), library_(library) {}
    PlaylistTab* createView();
    void run();

signals:
    void libraryUpdated(QList<LibraryEvent> events);

public slots:
    void quit();
    void updateViews(LibraryEvent event);
private slots:
    void update();

private:
    QMutex mutex_;
    QQueue<LibraryEvent> events_;

    MainWindow& mainwindow_;
    Library& library_;
    QList<PlaylistTab*> views_;
};

#endif // VIEWMANAGER_H
