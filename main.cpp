#include <QtGui/QApplication>
#include <QDebug>
#include "ui/mainwindow.h"
#include "library/library.h"
#include "library/viewmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("fubar");
    QCoreApplication::setOrganizationName("fubar");

    qRegisterMetaType<LibraryEvent>("LibraryEvent");
    Library library;
    std::vector<QString> folders;
    folders.push_back("/home/bogdan/music_test");
    library.setMusicFolders(folders);
    library.dumpDatabase();
    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));

    MainWindow w;
    w.show();

    ViewManager viewManager(w, library);
    viewManager.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &viewManager, SLOT(quit()));
    QObject::connect(&library, SIGNAL(libraryChanged(LibraryEvent)), &viewManager, SLOT(updateViews(LibraryEvent)));
    w.addView(viewManager.createView(), "All");

    int ret = a.exec();
    viewManager.wait();
    library.wait();
    return ret;
}
