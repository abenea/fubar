#include "console.h"
#include <QtGlobal>
#include <QMetaType>
#include <iostream>

Console* Console::instance_ = nullptr;

Console::Console(): QObject()
{
}

Console* Console::instance()
{
    qRegisterMetaType<QtMsgType>("QtMsgType");
    if (!instance_) {
        instance_ = new Console();
        qInstallMsgHandler([](QtMsgType type, const char *msg) { instance_->update(type,  msg); });
    }
    return instance_;
}

void Console::update(QtMsgType type, const char* message)
{
    emit updated(type, QString::fromUtf8(message));
}

#include "console.moc"