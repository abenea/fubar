#include "library/library.h"
#include "player/mpvaudiooutput.h"
#include "ui/audioplayer.h"
#include "ui/mainwindow.h"
#include "ui/pluginmanager.h"
#include "ui/unixsignalshandler.h"
#include "util.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QtWidgets/QApplication>
#include <cstring>
#include <fcntl.h>
#include <glyr/glyr.h>
#include <sys/file.h>

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

    qRegisterMetaType<AudioState>("AudioState");
    qRegisterMetaType<LibraryEvent>("LibraryEvent");

    glyr_init();
    atexit(glyr_cleanup);

    Library library;
    MpvAudioOutput audioOutput;
    AudioPlayer player(&library, &audioOutput);
    MainWindow w(player);
    w.show();

    UnixSignalsHandler signalsHandler(&w);

    PluginManager pluginmanager(player);

    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));

    int ret = a.exec();
    library.wait();
    return ret;
}
