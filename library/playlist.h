#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <boost/shared_ptr.hpp>
#include <QString>
#include <QMap>
#include <QDir>

namespace proto {
    class Track;
}

struct Track
{
    QString location;
    struct AudioProperties {
        int length;
        int bitrate;
        int samplerate;
        int channels;
    } audioproperties;
    QMap<QString, QString> metadata;

    Track() {}
    Track(proto::Track ptrack);
};

class Playlist
{
public:
    QList<boost::shared_ptr<Track> > tracks;

    void load(const char *fileName);
    void addDirectory(const QString &path);
    void addFile(const QFileInfo &file);
};

#endif // PLAYLIST_H
