#include <QtGui/QApplication>
#include <QDebug>
#include "ui/mainwindow.h"
#include "library/library.h"
#include "ui/globalshortcutengine.h"
#include <QxtGlobalShortcut>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("fubar");
    QCoreApplication::setOrganizationName("fubar");

    qRegisterMetaType<LibraryEvent>("LibraryEvent");
    Library library;

    MainWindow w(library);
    GlobalShortcutEngine gl(w);
    QxtGlobalShortcut* shortcut = new QxtGlobalShortcut(&w);
    QObject::connect(shortcut, SIGNAL(activated()), &w, SLOT(ShowHide()));
    shortcut->setShortcut(QKeySequence("Ctrl+Shift+F12"));
    w.show();
    w.ShowHide();
    w.ShowHide();

    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));

    int ret = a.exec();
    library.wait();
    return ret;
}
