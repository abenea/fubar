#pragma once

#include <QJsonDocument>
#include <QProcess>

// Run a command in a subprocess and parse its JSON stdout.
//
// The standard output is assumed to only have JSON documents separated by newlines.
class JsonProcess : public QObject {
    Q_OBJECT
public:
    void start(const QString &program, const QStringList &arguments);
    bool waitForFinished(int msecs = 30000);

    QByteArray stderr() { return stderr_; }

signals:
    // Error running the subprocess.
    void errorOccurred(QProcess::ProcessError error);
    // Subprocess emitted an invalid JSON document. No further documents will be emitted.
    void parseError(QJsonParseError error);
    // Subprocess emitted a valid JSON document.
    void document(const QJsonDocument &doc);
    // Subprocess returned exitCode.
    void finished(int exitCode);

private:
    QProcess proc_;
    QByteArray stdout_;
    QByteArray stderr_;
    QJsonParseError parseError_{.error = QJsonParseError::NoError};

private slots:
    void readyRead();
    void readyReadStandardError();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
};
