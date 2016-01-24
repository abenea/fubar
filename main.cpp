#include "util.h"
#include "library/library.h"
#include "ui/audioplayer.h"
#include "ui/pluginmanager.h"
#include "ui/mainwindow.h"
#include "ui/unixsignalshandler.h"
#include <QtGui/QApplication>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <sys/file.h>
#include <fcntl.h>
#include <cstring>

int main(int argc, char *argv[]) {
    QCoreApplication::setApplicationName("fubar");
    QCoreApplication::setOrganizationName("fubar");

    // Single instance
    QString settings_dir_path = settingsDirFilePath("");
    QFileInfo settings_dir = QFileInfo();
    if (!settings_dir.exists()) {
        if (!QDir().mkpath(settings_dir_path))
            qFatal("Cannot create settings directory %s", settings_dir_path.toStdString().c_str());
    } else if (!settings_dir.isDir()) {
        qFatal("%s exists, but it is not a directory ", settings_dir_path.toStdString().c_str());
    }
    QString lock_path = settingsDirFilePath("lock");
    int fd = open(lock_path.toStdString().c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        qDebug() << "Cant open lock file " << lock_path << ": " << strerror(errno);
        return 1;
    } else {
        if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
            if (errno == EWOULDBLOCK) {
                qDebug() << "fubar already running";
                return 0;
            } else {
                qDebug() << "flock() failed: " << strerror(errno);
                return 1;
            }
        }
    }

    QApplication a(argc, argv);

    qRegisterMetaType<LibraryEvent>("LibraryEvent");
    Library library;
    AudioPlayer player(&library, argc > 1 ? Backend::phonon : Backend::mpv);
    MainWindow w(player);
    w.show();

    UnixSignalsHandler signalsHandler(&w);

    PluginManager pluginmanager(player);

    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));

    int ret = a.exec();
    library.wait();
    return ret;
}
