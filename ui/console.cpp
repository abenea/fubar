#include "console.h"
#include <QMetaType>
#include <QtGlobal>
#include <iostream>

Console *Console::instance_ = nullptr;

Console::Console() : QObject() {}

Console *Console::instance() {
    qRegisterMetaType<QtMsgType>("QtMsgType");
    if (!instance_) {
        instance_ = new Console();
        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &logContext,
                                  const QString &msg) { instance_->update(type, msg); });
    }
    return instance_;
}

void Console::update(QtMsgType type, const QString &message) { emit updated(type, message); }
