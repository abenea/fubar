#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <map>

class ConfigWindow;

class Config : public QObject {
    Q_OBJECT
public:
    void set(QString key, QVariant value);

signals:
    void keySet(QString key, QVariant value);

private:
    std::map<QString, QVariant> config_;
    friend ConfigWindow;
};
