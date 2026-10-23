#include "util.h"
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>

QString settingsDirFilePath(const char *fileName) {
    QSettings settings;
    return QFileInfo(settings.fileName()).absoluteDir().absoluteFilePath(fileName);
}

QString redactUrlCredentials(const QString &text) {
    static const QRegularExpression credentials(
        QStringLiteral("([?&](?:u|t|s|p|apiKey)=)[^&#\\s\"'<>]*"));
    return QString(text).replace(credentials, QStringLiteral("\\1***"));
}

QString formatDuration(qint64 seconds) {
    return QString::number(seconds / 60) + ':' +
           QString::number(seconds % 60).rightJustified(2, QLatin1Char('0'));
}

QString countText(int n, const QString &singular, const QString &plural) {
    return QLocale().toString(n) + ' ' + (n == 1 ? singular : plural);
}
