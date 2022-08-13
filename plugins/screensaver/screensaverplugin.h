#pragma once
#include "plugins/PluginInterface.h"
#include <QObject>

class ScreensaverPlugin : public QObject, public PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "fubar.ScreenSaver" FILE "screensaver.json")

public:
    explicit ScreensaverPlugin(QObject *parent = 0);
    ~ScreensaverPlugin();

    void init(QObject &fubarApp);
    void deinit();

signals:
    void pause();

private slots:
    void stateChanged(bool active);

private:
    QObject *fubar_;
};