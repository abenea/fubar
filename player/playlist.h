#pragma once

#include "library/track_forward.h"
#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QUrl>

class Playlist {
public:
    QList<PTrack> tracks;
    bool synced;

    void addDirectory(const QString &path);
    void addFile(const QString &path);
    void addUrls(const QList<QUrl> &urls);

    void deserialize(const QByteArray &bytes);
    void serialize(QByteArray &bytes) const;

private:
    void addM3u(const QString &path);
};
