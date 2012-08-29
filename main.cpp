#include "ui/mainwindow.h"
#include "library/library.h"
#include "plugins/PluginInterface.h"
#include <QtGui/QApplication>
#include <QDebug>
#include <QDir>
#include <QPluginLoader>

void loadPlugins(MainWindow& mw)
{
    auto pluginsDir = QDir(qApp->applicationDirPath());
    pluginsDir.cd("lib");

    foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
        QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
         QObject *plugin = loader.instance();
         if (plugin) {
             qDebug() << "Loading plugin " << fileName;
             PluginInterface *fubarPlugin = qobject_cast<PluginInterface*>(plugin);
             if (fubarPlugin) {
                fubarPlugin->init(mw);
             } else {
                 qDebug() << "Can't load plugin " << loader.errorString();
             }
         }
     }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("fubar");
    QCoreApplication::setOrganizationName("fubar");

    qRegisterMetaType<LibraryEvent>("LibraryEvent");
    Library library;

    MainWindow w(library);
    w.show();

    loadPlugins(w);

    library.start();
    QObject::connect(&a, SIGNAL(lastWindowClosed()), &library, SLOT(quit()));

    int ret = a.exec();
    library.wait();
    return ret;
}
