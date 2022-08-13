#include "screensaverplugin.h"
#include <QDBusConnection>
#include <QDebug>

namespace {
const char *DBUS_INTERFACES[][3] = {
    {"org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver"},
    {"org.cinnamon.ScreenSaver", "/org/cinnamon/ScreenSaver", "org.cinnamon.ScreenSaver"},
};
}

ScreensaverPlugin::ScreensaverPlugin(QObject *parent) : QObject(parent) {
    qDebug() << "[Screensaver] ScreensaverPlugin::ScreensaverPlugin()";
}

ScreensaverPlugin::~ScreensaverPlugin() {
    deinit();
    fubar_ = 0;
}

void ScreensaverPlugin::init(QObject &fubarApp) {
    fubar_ = &fubarApp;
    for (auto &dbus : DBUS_INTERFACES) {
        QDBusConnection::sessionBus().connect(dbus[0], dbus[1], dbus[2], "ActiveChanged", this,
                                              SLOT(stateChanged(bool)));
    }
    connect(this, SIGNAL(pause()), fubar_, SLOT(pause()));
}

void ScreensaverPlugin::deinit() {
    for (auto &dbus : DBUS_INTERFACES) {
        QDBusConnection::sessionBus().disconnect(dbus[0], dbus[1], dbus[2], "ActiveChanged", this,
                                                 SLOT(stateChanged(bool)));
    }
    disconnect(this, 0, fubar_, 0);
}

void ScreensaverPlugin::stateChanged(bool active) {
    if (active)
        emit pause();
}
