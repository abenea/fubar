#pragma once
#include <QString>

QString settingsDirFilePath(const char *fileName);

// Replaces the values of the Subsonic auth query parameters (u, t, s, p, apiKey) with "***".
QString redactUrlCredentials(const QString &text);

// "m:ss"
QString formatDuration(qint64 seconds);
// E.g. "1,234 tracks", with the digits grouped as by QLocale()
QString countText(int n, const QString &singular, const QString &plural);
