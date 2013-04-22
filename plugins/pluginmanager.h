#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "ui/mainwindow.h"
#include "player/audioplayer.h"
#include <map>
#include <vector>
#include <memory>

class QPluginLoader;

class PluginManager
{
public:
    PluginManager(AudioPlayer& player);

    static PluginManager *instance;

    void enablePlugin(QString name);
    void disablePlugin(QString name);
    void configurePlugin(QString name);
    std::vector<QString> getPlugins();

private:
    AudioPlayer& player_;
    typedef std::map<QString, std::shared_ptr<QPluginLoader>> PluginMap;
    PluginMap plugins_;
};

#endif // PLUGINMANAGER_H
