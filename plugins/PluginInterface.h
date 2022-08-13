#pragma once

#include <QObject>
#include <QtPlugin>

class PluginInterface {
public:
    virtual ~PluginInterface() {}
    virtual void init(QObject &fubarApp) = 0;
    virtual void deinit() = 0;
    virtual void configure() {}
};

QT_BEGIN_NAMESPACE
Q_DECLARE_INTERFACE(PluginInterface, "com.PluginInterface/1.0")
QT_END_NAMESPACE
