#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "track.h"
#include <QString>
#include <QFileInfo>

class Playlist
{
public:
    QList<std::shared_ptr<Track> > tracks;

    void load(const char *fileName);
    void addDirectory(const QString &path);
    void addFile(const QFileInfo &file);
    void addFiles(const QStringList& files);
};

#endif // PLAYLIST_H
