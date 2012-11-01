#ifndef TRACK_H
#define TRACK_H

#include <QString>
#include <QMap>
#include <cstdio>
#include <memory>

namespace proto {
    class Track;
}

class Track
{
public:
    QString location;
    struct AudioProperties {
        int length;
        int bitrate;
        int samplerate;
        int channels;
    } audioproperties;
    QMap<QString, QString> metadata;
    uint mtime;

    Track() {}
    Track(proto::Track ptrack);

    QString path() { return location; }

    void fillProtoTrack(proto::Track& ptrack);
};

typedef std::shared_ptr<Track> PTrack;

#endif // TRACK_H
