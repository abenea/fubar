#include "pluginmanager.h"
#include "plugins/PluginInterface.h"
#include <QDir>
#include <QPluginLoader>

PluginManager *PluginManager::instance = 0;

PluginManager::PluginManager(MainWindow& mainWindow)
    : mainWindow_(mainWindow)
{
    auto pluginsDir = QDir(QCoreApplication::applicationDirPath());
    pluginsDir.cd("lib");

    foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
        std::shared_ptr<QPluginLoader> loader(new QPluginLoader(pluginsDir.absoluteFilePath(fileName)));
        QObject *plugin = loader->instance();
        if (plugin) {
            PluginInterface *fubarPlugin = qobject_cast<PluginInterface*>(plugin);
            if (fubarPlugin) {
                qDebug() << "Found plugin " << fileName;
                fubarPlugin->init(mainWindow_);
                plugins_.insert(std::make_pair(fileName, loader));
//                disablePlugin(fileName);
            } else {
                qDebug() << "Can't load plugin " << loader->errorString();
            }
        }
     }

     instance = this;
}

void PluginManager::disablePlugin(QString name)
{
    PluginMap::iterator it = plugins_.find(name);
    if (it != plugins_.end()) {
        if (!it->second->isLoaded())
            return;
        qDebug() << "Unloading plugin " << it->second->fileName();
        if (!it->second->unload())
            qDebug() << "Error unloading plugin " << it->second->fileName() << ". "<< it->second->errorString();
    } else {
        qDebug() << "Cannot unload " << name << ". No such plugin.";
    }
}

void PluginManager::enablePlugin(QString name)
{
    PluginMap::iterator it = plugins_.find(name);
    if (it != plugins_.end()) {
        qDebug() << "Loading plugin " << it->second->fileName();
        if (it->second->isLoaded())
            return;
        QObject *plugin = it->second->instance();
        if (plugin) {
            PluginInterface *fubarPlugin = qobject_cast<PluginInterface*>(plugin);
            if (fubarPlugin) {
                fubarPlugin->init(mainWindow_);
            } else {
                qDebug() << "Can't load plugin " << it->second->fileName() << ". " << it->second->errorString();
            }
        } else {
            qDebug() << "Instance is 0 for Plugin " << it->second->fileName();
        }
    } else {
        qDebug() << "Cannot load " << name << ". No such plugin.";
    }
}

std::vector<QString> PluginManager::getPlugins()
{
    std::vector<QString> plugins;
    for (PluginMap::const_iterator it = plugins_.begin(); it != plugins_.end(); ++it) {
        plugins.push_back(it->first);
    }
    return plugins;
}
