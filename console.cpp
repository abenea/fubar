#include "console.h"
#include <QtGlobal>

Console* Console::instance_ = nullptr;

Console::Console(): QObject()
{
    maxLength_ = 1024 * 1024;
}

Console* Console::instance()
{
    if (!instance_) {
        instance_ = new Console();
        qInstallMsgHandler([](QtMsgType type, const char *msg) { instance_->update(type,  msg); });
    }
    return instance_;
}

void Console::update(QtMsgType /*type*/, const char* message)
{
    text_.append(QString("\n"));
    text_.append(QString::fromUtf8(message));
    if (text_.size() > maxLength_)
        text_ = text_.right(maxLength_);
    emit updated();
}

#include "console.moc"