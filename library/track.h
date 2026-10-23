#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <memory>

namespace proto {
class Track;
}

class Track {
public:
    QString location;
    struct AudioProperties {
        int length = 0;
        int bitrate = 0;
        int samplerate = 0;
        int channels = 0;
    } audioproperties;
    QMap<QString, QString> metadata;
    uint mtime = 0;

    Track() {}
    Track(const proto::Track &ptrack);

    QString path() const { return metadata.value(QStringLiteral("_path"), location); }
    // The title, else the file name without its extension
    QString displayTitle() const;
    // An absolute path, not a URL or a server location
    bool isLocalFile() const;
    QString audioFormat() const;

    void fillProtoTrack(proto::Track &ptrack);

    void dump();

    // Url hax
    bool isUrl();
    bool updateDuration(int duration);
    bool updateMetadata(const QString &title, const QString &audioFormat, int sampleRate);

    // CUE TRAX HAX
    bool isCueTrack();
    QString cueTrackLocation();
    void updateAudioInfo(std::shared_ptr<Track> track);

    bool isCue();
    QList<std::shared_ptr<Track>> getCueTracks();
    void setCueTracks(QList<std::shared_ptr<Track>> tracks);
    int cueOffset(); // in ms
};
