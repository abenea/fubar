#include "util.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>

QString settingsDirFilePath(const char *fileName) {
    QSettings settings;
    return QFileInfo(settings.fileName()).absoluteDir().absoluteFilePath(fileName);
}