#pragma once
#include <QObject>

class Console : public QObject
{
    Q_OBJECT
public:
    static Console* instance();

    QString text() { return text_; }

signals:
    void updated();

private:
    Console();
    void update(QtMsgType type, const char* message);

    static Console* instance_;
    QString text_;
    int maxLength_;
};
