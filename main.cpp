#include <QtGui/QApplication>
#include <QDebug>
#include "ui/mainwindow.h"
#include "library/library.h"
#include "ui/globalshortcutengine.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("fubar");
    QCoreApplication::setOrganizationName("fubar");

    qRegisterMetaType<LibraryEvent>("LibraryEvent");
    Library library;

    MainWindow w(library);
    GlobalShortcutEngine gl(w);
    w.show();

    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));

    int ret = a.exec();
    library.wait();
    return ret;
}
