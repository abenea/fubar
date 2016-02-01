#pragma once

#include <QString>
#include <QMap>
#include <memory>
#include <QList>

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

    void dump();

    // Url hax
    bool isUrl();
    void updateDuration(int duration);

    // CUE TRAX HAX
    bool isCueTrack();
    QString cueTrackLocation();
    void updateAudioInfo(std::shared_ptr<Track> track);

    bool isCue();
    QList<std::shared_ptr<Track>> getCueTracks();
    void setCueTracks(QList<std::shared_ptr<Track>> tracks);
    int cueOffset(); // in ms
};
