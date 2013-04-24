#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "library/track_forward.h"
#include <QString>
#include <QFileInfo>

class Playlist
{
public:
    QList<PTrack> tracks;
    QString name;
    bool synced;

    void load(const char *fileName);
    void addDirectory(const QString &path);
    void addFile(const QFileInfo &file);
    void addFiles(const QStringList& files);
};

#endif // PLAYLIST_H
