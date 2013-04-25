#pragma once
#include <QObject>

class Console : public QObject
{
    Q_OBJECT
public:
    static Console* instance();

    QString text() { return text_; }
    void update(QtMsgType type, const char* message);

signals:
    void updated();

private:
    Console();

    static Console* instance_;
    QString text_;
    int maxLength_;
};
