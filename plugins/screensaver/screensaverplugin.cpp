#include "screensaverplugin.h"
#include <QDebug>
#include <QDBusConnection>

namespace {
const QString SERVICE = "org.freedesktop.ScreenSaver";
const QString PATH = "/org/freedesktop/ScreenSaver";
const QString INTERFACE = "org.freedesktop.ScreenSaver";
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
    QDBusConnection::sessionBus().connect(
        SERVICE, PATH, INTERFACE, "ActiveChanged", this, SLOT(stateChanged(bool)));
    connect(this, SIGNAL(pause()), fubar_, SLOT(pause()));
}

void ScreensaverPlugin::deinit() {
    QDBusConnection::sessionBus().disconnect(
        SERVICE, PATH, INTERFACE, "ActiveChanged", this, SLOT(stateChanged(bool)));
    disconnect(this, 0, fubar_, 0);
}

void ScreensaverPlugin::stateChanged(bool active) {
    if (active)
        emit pause();
}
