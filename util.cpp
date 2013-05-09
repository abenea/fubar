#include "util.h"
#include <QSettings>
#include <QFileInfo>
#include <QDir>

QString settingsDirFilePath(const char* fileName)
{
    QSettings settings;
    return QFileInfo(settings.fileName()).absoluteDir().absoluteFilePath(fileName);
}