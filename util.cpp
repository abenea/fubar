#include "util.h"
#include <QSettings>
#include <QFileInfo>
#include <QDir>

const char* settingsDirFilePath(const char* fileName)
{
    QSettings settings;
    return QFileInfo(settings.fileName()).absoluteDir().absoluteFilePath(fileName).toStdString().c_str();
}