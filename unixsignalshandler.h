#ifndef UNIXSIGNALSHANDLER_H
#define UNIXSIGNALSHANDLER_H

#include <QObject>
#include <QSocketNotifier>

class UnixSignalsHandler : public QObject
{
    Q_OBJECT
public:
    UnixSignalsHandler(QObject *parent = 0);

    // Unix signal handlers.
    static void termSignalHandler(int unused);

public slots:
    // Qt signal handlers.
    void handleSigTerm();

private:
    static int sigtermFd_[2];
    QSocketNotifier *snTerm_;
};

#endif // UNIXSIGNALSHANDLER_H
