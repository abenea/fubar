#include "util.h"
#include "ui/mainwindow.h"
#include "library/library.h"
#include "plugins/pluginmanager.h"
#include <QtGui/QApplication>
#include <QDebug>
#include <sys/file.h>
#include <fcntl.h>
#include <cstring>



int main(int argc, char *argv[])
{
    // Single instance
    int fd = open(settingsDirFilePath("lock"), O_RDONLY | O_CREAT);
    if (fd == -1) {
        qDebug()  << "Cant open lock file " << settingsDirFilePath("lock") << ": " << strerror(errno);
        return 1;
    }
    else {
        if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
            if (errno == EWOULDBLOCK) {
                qDebug() << "fubar already running";
                return 0;
            }
            else {
                qDebug() << "flock() failed: " << strerror(errno);
                return 1;
            }
        }
    }

    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("fubar");
    QCoreApplication::setOrganizationName("fubar");

    qRegisterMetaType<LibraryEvent>("LibraryEvent");
    Library library;

    MainWindow w(library);
    w.show();

    PluginManager pluginmanager(w);

    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));

    int ret = a.exec();
    library.wait();
    return ret;
}
