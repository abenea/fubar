#include "ui/pluginmanager.h"
#include "plugins/PluginInterface.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QPluginLoader>

PluginManager *PluginManager::instance = 0;

PluginManager::PluginManager(AudioPlayer &player) : player_(player) {
    auto pluginDirs = QVector<QString>{
        // Install path
        QCoreApplication::applicationDirPath() + "/../lib/fubar/",
        // Build tree paths
        QCoreApplication::applicationDirPath() + "/plugins/lastfm",
        QCoreApplication::applicationDirPath() + "/plugins/screensaver",
    };
    for (QString dirName : pluginDirs) {
        auto dir = QDir(dirName);
        qDebug() << "Looking for plugins in " << dir.path();
        for (QString fileName : dir.entryList(QDir::Files)) {
            std::shared_ptr<QPluginLoader> loader(
                new QPluginLoader(dir.absoluteFilePath(fileName)));
            QObject *plugin = loader->instance();
            if (plugin) {
                PluginInterface *fubarPlugin = qobject_cast<PluginInterface *>(plugin);
                if (fubarPlugin) {
                    qDebug() << "Found plugin " << fileName;
                    plugins_.insert(std::make_pair(fileName, loader));
                    enablePlugin(fileName);
                } else {
                    qDebug() << "Can't load plugin " << loader->errorString();
                }
            }
        }
    }

    instance = this;
}

void PluginManager::disablePlugin(QString name) {
    if (!isEnabled(name))
        return;
    PluginMap::iterator it = plugins_.find(name);
    if (it != plugins_.end()) {
        qDebug() << "Disable plugin" << it->second->fileName();
        PluginInterface *fubarPlugin = qobject_cast<PluginInterface *>(it->second->instance());
        if (fubarPlugin) {
            fubarPlugin->deinit();
            enabled_.erase(name);
        }
    } else {
        qDebug() << "Cannot disable" << name << "No such plugin.";
    }
}

void PluginManager::enablePlugin(QString name) {
    if (isEnabled(name))
        return;
    PluginMap::iterator it = plugins_.find(name);
    if (it != plugins_.end()) {
        qDebug() << "Enable plugin" << it->second->fileName();
        QObject *plugin = it->second->instance();
        if (plugin) {
            PluginInterface *fubarPlugin = qobject_cast<PluginInterface *>(plugin);
            if (fubarPlugin) {
                fubarPlugin->init(player_);
                enabled_.insert(name);
            }
        }
    } else {
        qDebug() << "Cannot enable" << name << "No such plugin.";
    }
}

void PluginManager::configurePlugin(QString name) {
    PluginMap::iterator it = plugins_.find(name);
    if (it != plugins_.end()) {
        if (it->second->isLoaded()) {
            QObject *plugin = it->second->instance();
            if (plugin) {
                PluginInterface *fubarPlugin = qobject_cast<PluginInterface *>(plugin);
                if (fubarPlugin) {
                    fubarPlugin->configure();
                } else {
                    qDebug() << "Bad plugin " << it->second->fileName();
                }
            } else {
                qDebug() << "Instance is 0 for Plugin " << it->second->fileName();
            }
        } else {
            qDebug() << "Cannot configure " << name << ". Plugin not loaded.";
        }
    } else {
        qDebug() << "Cannot configure " << name << ". No such plugin.";
    }
}

std::vector<QString> PluginManager::getPlugins() {
    std::vector<QString> plugins;
    for (PluginMap::const_iterator it = plugins_.begin(); it != plugins_.end(); ++it) {
        plugins.push_back(it->first);
    }
    return plugins;
}

bool PluginManager::isEnabled(QString name) { return enabled_.find(name) != enabled_.end(); }
