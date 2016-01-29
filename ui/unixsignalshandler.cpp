#include "unixsignalshandler.h"
#include "ui/mainwindow.h"
#include <QDebug>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

int UnixSignalsHandler::sigtermFd_[2];

static void setup_unix_signal_handlers()
{
    struct sigaction term;

    term.sa_handler = UnixSignalsHandler::termSignalHandler;
    sigemptyset(&term.sa_mask);
    term.sa_flags |= SA_RESTART;

    for (auto signal : {SIGTERM, SIGINT}) {
        if (sigaction(signal, &term, 0) == -1)
            qDebug() << "sigaction failed for" << strsignal(signal) << strerror(errno);
    }
}


UnixSignalsHandler::UnixSignalsHandler(QObject *parent)
    : QObject(parent)
{
    if (!::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd_)) {
        snTerm_ = new QSocketNotifier(sigtermFd_[1], QSocketNotifier::Read, this);
        connect(snTerm_, SIGNAL(activated(int)), this, SLOT(handleSigTerm()));
        setup_unix_signal_handlers();
    } else {
        qDebug() << "socketpair failed" << strerror(errno);
    }
}

void UnixSignalsHandler::termSignalHandler(int)
{
    char a = 1;
    ::write(sigtermFd_[0], &a, sizeof(a));
}

void UnixSignalsHandler::handleSigTerm()
{
    snTerm_->setEnabled(false);
    char tmp;
    ::read(sigtermFd_[1], &tmp, sizeof(tmp));

    // do Qt stuff
    MainWindow::instance->close();

    snTerm_->setEnabled(true);
}
