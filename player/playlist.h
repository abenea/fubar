#pragma once

#include "library/track_forward.h"
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QByteArray>

class Playlist
{
public:
    QList<PTrack> tracks;
    bool synced;

    void addDirectory(const QString &path);
    void addFile(const QFileInfo &file);
    void addFiles(const QStringList& files);

    void deserialize(const QByteArray& bytes);
    void serialize(QByteArray& bytes) const;
};
