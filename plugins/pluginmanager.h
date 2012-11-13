#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "ui/mainwindow.h"
#include <map>
#include <vector>
#include <memory>

class QPluginLoader;

class PluginManager
{
public:
    PluginManager(MainWindow& mainWindow);

    static PluginManager *instance;

    void enablePlugin(QString name);
    void disablePlugin(QString name);
    std::vector<QString> getPlugins();

private:
    MainWindow& mainWindow_;
    typedef std::map<QString, std::shared_ptr<QPluginLoader>> PluginMap;
    PluginMap plugins_;
};

#endif // PLUGINMANAGER_H
