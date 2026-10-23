#include "navidrome/navidrometrack.h"

#include "library/track.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

namespace navidrome {

const QString kLocationPrefix = QStringLiteral("navidrome:");

bool isNavidromeLocation(const QString &location) { return location.startsWith(kLocationPrefix); }

QString locationForSong(const QString &songId) { return kLocationPrefix + songId; }

QString songIdFromLocation(const QString &location) {
    if (!isNavidromeLocation(location))
        return QString();
    return location.mid(kLocationPrefix.size());
}

QString idOf(const QJsonObject &object) { return object.value("id").toVariant().toString(); }

namespace {

QString albumArtistOf(const QJsonObject &song) {
    QString albumArtist = song.value("displayAlbumArtist").toString();
    if (!albumArtist.isEmpty())
        return albumArtist;
    QStringList names;
    for (const QJsonValue &artist : song.value("albumArtists").toArray()) {
        QString name = artist.toObject().value("name").toString();
        if (!name.isEmpty())
            names << name;
    }
    return names.join(", ");
}

QString synthesizePath(const QString &id,
                       const QString &artist,
                       const QString &album,
                       int disc,
                       int track,
                       const QString &title,
                       const QString &suffix) {
    QStringList parts;
    if (!artist.isEmpty())
        parts << artist;
    if (!album.isEmpty())
        parts << album;
    QStringList name;
    if (track > 0) {
        QString number = QStringLiteral("%1").arg(track, 2, 10, QLatin1Char('0'));
        name << (disc > 0 ? QString::number(disc) + "-" + number : number);
    }
    if (!title.isEmpty())
        name << title;
    QString fileName = name.isEmpty() ? id : name.join(" - ");
    if (!suffix.isEmpty())
        fileName += "." + suffix;
    parts << fileName;
    return parts.join("/");
}

} // namespace

PTrack trackFromSong(const QJsonObject &song) {
    QString id = idOf(song);
    if (id.isEmpty() || song.value("isDir").toBool())
        return nullptr;

    PTrack track = std::make_shared<Track>();
    track->location = locationForSong(id);
    QMap<QString, QString> &metadata = track->metadata;
    auto setText = [&](const QString &key, const QString &value) {
        if (!value.isEmpty())
            metadata[key] = value;
    };
    auto setNumber = [&](const QString &key, int value) {
        if (value > 0)
            metadata[key] = QString::number(value);
    };

    QString title = song.value("title").toString();
    QString album = song.value("album").toString();
    QString artist = song.value("artist").toString();
    if (artist.isEmpty())
        artist = song.value("displayArtist").toString();
    QString albumArtist = albumArtistOf(song);
    QString suffix = song.value("suffix").toString();
    int year = song.value("year").toInt();
    int number = song.value("track").toInt();
    int disc = song.value("discNumber").toInt();

    setText("title", title);
    setText("album", album);
    setText("artist", artist);
    setText("album artist", albumArtist);
    setText("genre", song.value("genre").toString());
    setText("comment", song.value("comment").toString());
    setNumber("year", year);
    setNumber("track", number);
    setNumber("disc", disc);
    setText("audio-format", suffix.toUpper());

    QJsonObject replayGain = song.value("replayGain").toObject();
    auto setReplayGain = [&](const QString &key, const char *field, bool peak) {
        QJsonValue value = replayGain.value(field);
        if (!value.isDouble())
            return;
        metadata[key] = peak ? QString::number(value.toDouble(), 'f', 6)
                             : QString::number(value.toDouble(), 'f', 2) + " dB";
    };
    setReplayGain("REPLAYGAIN_TRACK_GAIN", "trackGain", false);
    setReplayGain("REPLAYGAIN_ALBUM_GAIN", "albumGain", false);
    setReplayGain("REPLAYGAIN_TRACK_PEAK", "trackPeak", true);
    setReplayGain("REPLAYGAIN_ALBUM_PEAK", "albumPeak", true);

    QString path = song.value("path").toString();
    if (path.isEmpty())
        path = synthesizePath(id, albumArtist.isEmpty() ? artist : albumArtist, album, disc, number,
                              title, suffix);
    metadata["_path"] = path;

    track->audioproperties.length = song.value("duration").toInt();
    track->audioproperties.bitrate = song.value("bitRate").toInt();
    track->audioproperties.samplerate = song.value("samplingRate").toInt();
    track->audioproperties.channels = song.value("channelCount").toInt();
    return track;
}

QList<PTrack> tracksFromSongs(const QJsonArray &songs) {
    QList<PTrack> tracks;
    for (const QJsonValue &song : songs) {
        if (PTrack track = trackFromSong(song.toObject()))
            tracks.append(track);
    }
    return tracks;
}

} // namespace navidrome
