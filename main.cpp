#include "util.h"
#include "ui/mainwindow.h"
#include "phononaudiooutput.h"
#include "library/library.h"
#include "plugins/pluginmanager.h"
#include "unixsignalshandler.h"
#include <QtGui/QApplication>
#include <QDebug>
#include <sys/file.h>
#include <fcntl.h>
#include <cstring>



int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("fubar");
    QCoreApplication::setOrganizationName("fubar");

    // Single instance
    int fd = open(settingsDirFilePath("lock"), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
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

    qRegisterMetaType<LibraryEvent>("LibraryEvent");
    Library library;
    PhononAudioOutput audio;
    MainWindow w(&library, &audio);
    w.show();

    UnixSignalsHandler signalsHandler(&w);

    PluginManager pluginmanager(w);

    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));

    int ret = a.exec();
    library.wait();
    return ret;
}
