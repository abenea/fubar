#ifndef TRACK_H
#define TRACK_H

#include <QString>
#include <QMap>
#include <boost/shared_ptr.hpp>
#include <cstdio>

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
    int32_t mtime;

    Track() {}
    Track(proto::Track ptrack);

    QString path() { return location; }

    void fillProtoTrack(proto::Track& ptrack);
};


#endif // TRACK_H
