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
        int length = 0;
        int bitrate = 0;
        int samplerate = 0;
        int channels = 0;
    } audioproperties;
    QMap<QString, QString> metadata;
    uint mtime;

    Track() {}
    Track(proto::Track ptrack);

    QString path() const { return location; }
    QString audioFormat() const;

    void fillProtoTrack(proto::Track& ptrack);

    void dump();

    // Url hax
    bool isUrl();
    bool updateDuration(int duration);
    bool updateMetadata(const QString& title, const QString& audioFormat, int sampleRate);

    // CUE TRAX HAX
    bool isCueTrack();
    QString cueTrackLocation();
    void updateAudioInfo(std::shared_ptr<Track> track);

    bool isCue();
    QList<std::shared_ptr<Track>> getCueTracks();
    void setCueTracks(QList<std::shared_ptr<Track>> tracks);
    int cueOffset(); // in ms
};
