#include "player/jsonprocess.h"
#include <iostream>
#include <QDebug>

void JsonProcess::start(const QString &program, const QStringList &arguments) {
    qWarning() << program << arguments.join(" ");
    qRegisterMetaType<QProcess::ProcessError>();
    proc_.start(program, arguments);
    connect(&proc_, &QProcess::readyRead, this, &JsonProcess::readyRead);
    connect(&proc_, &QProcess::readyReadStandardError, this, &JsonProcess::readyReadStandardError);
    connect(&proc_, &QProcess::errorOccurred, this, &JsonProcess::errorOccurred);
    connect(
        &proc_,
        static_cast<void (QProcess::*)(int, QProcess::ExitStatus exitStatus)>(&QProcess::finished),
        this, &JsonProcess::processFinished);
    // TODO: if proc_ fails to start no signals are emitted, so it won't get cleaned up.
}

bool JsonProcess::waitForFinished(int msecs) {
    bool ret = proc_.waitForFinished(msecs);
    if (!ret)
        qWarning() << "fail to wait: " << proc_.error();
    return ret;
}

void JsonProcess::readyRead() {
    while (proc_.canReadLine()) {
        QByteArray line = proc_.readLine();
        // Keep accumulating input line by line until it parses successfully.
        stdout_ += line;
        QJsonDocument doc = QJsonDocument::fromJson(stdout_, &parseError_);
        if (parseError_.error == QJsonParseError::NoError) {
            emit document(doc);
            stdout_.clear();
        }
    }
}

void JsonProcess::readyReadStandardError() { stderr_ += proc_.readAllStandardError(); }

void JsonProcess::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (parseError_.error != QJsonParseError::NoError)
        emit parseError(parseError_);
    emit finished(exitCode);
}
