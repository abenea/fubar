#pragma once

#include "library/track_forward.h"
#include <QString>
#include <QUrl>
#include <QStringList>
#include <QFileInfo>
#include <QByteArray>

class Playlist
{
public:
    QList<PTrack> tracks;
    bool synced;

    void addDirectory(const QString &path);
    void addFile(const QString &path);
    void addUrls(const QList<QUrl>& urls);

    void deserialize(const QByteArray& bytes);
    void serialize(QByteArray& bytes) const;

private:
    void addM3u(const QString &path);
};
