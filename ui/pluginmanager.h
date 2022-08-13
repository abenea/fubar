#pragma once

#include "audioplayer.h"
#include <map>
#include <memory>
#include <set>
#include <vector>

class QPluginLoader;

class PluginManager {
public:
    PluginManager(AudioPlayer &player);

    static PluginManager *instance;

    void enablePlugin(QString name);
    void disablePlugin(QString name);
    void configurePlugin(QString name);

    std::vector<QString> getPlugins();
    bool isEnabled(QString name);

private:
    AudioPlayer &player_;
    typedef std::map<QString, std::shared_ptr<QPluginLoader>> PluginMap;
    std::set<QString> enabled_;
    PluginMap plugins_;
};
