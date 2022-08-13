#pragma once
#include <QObject>

class Console : public QObject {
    Q_OBJECT
public:
    static Console *instance();

signals:
    void updated(QtMsgType type, QString message);

private:
    Console();
    void update(QtMsgType type, const QString &message);

    static Console *instance_;
};
