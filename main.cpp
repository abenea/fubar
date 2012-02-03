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

    MainWindow w;
    w.show();

    ViewManager viewManager(w, library);
    w.addView(viewManager.createView(), "All");

    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));
    std::vector<QString> folders;
    folders.push_back("/home/bogdan/music_test");
    library.setMusicFolders(folders);
    library.dumpDatabase();

    int ret = a.exec();
    library.wait();
    return ret;
}
