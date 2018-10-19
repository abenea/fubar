#include "track.h"
#include "track.pb.h"
#include "track_forward.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>

Track::Track(proto::Track ptrack)
{
    location = QString::fromUtf8(ptrack.location().c_str());
    if (ptrack.has_audioproperties()) {
        const proto::AudioProperties ap = ptrack.audioproperties();
        if (ap.has_bitrate())
            audioproperties.bitrate = ap.bitrate();
        if (ap.has_channels())
            audioproperties.channels = ap.channels();
        if (ap.has_length())
            audioproperties.length = ap.length();
        if (ap.has_samplerate())
            audioproperties.samplerate = ap.samplerate();
    }
    for (int i = 0; i < ptrack.metadata().fields_size(); i++)
    {
        const proto::MetadataItem &field = ptrack.metadata().fields(i);
        metadata[QString::fromUtf8(field.name().c_str())] = QString::fromUtf8(field.value().c_str());
    }
    if (ptrack.has_mtime())
        mtime = ptrack.mtime();
//    qDebug() << "LOADFROMDISK: " << metadata["title"] << " " << audioproperties.length;
}

void Track::fillProtoTrack(proto::Track& ptrack)
{
    ptrack.Clear();
    ptrack.set_mtime(mtime);
    ptrack.set_location(location.toUtf8());
    proto::AudioProperties* audioProperties = ptrack.mutable_audioproperties();
    audioProperties->set_length(audioproperties.length);
    audioProperties->set_bitrate(audioproperties.bitrate);
    audioProperties->set_samplerate(audioproperties.samplerate);
    audioProperties->set_channels(audioproperties.channels);
    proto::Metadata* meta = ptrack.mutable_metadata();
    for (QMap<QString, QString>::const_iterator it = metadata.begin(); it != metadata.end(); ++it) {
        proto::MetadataItem* item = meta->add_fields();
        item->set_name(it.key().toUtf8());
        item->set_value(it.value().toUtf8());
    }
}

QString Track::audioFormat() const {
    if (metadata.count("audio-format"))
        return metadata["audio-format"];
    return QFileInfo(location).suffix().toUpper();
}

bool Track::isCue()
{
    return location.toLower().endsWith(".cue");
}

bool Track::isCueTrack()
{
    return !location.toLower().endsWith(".cue") && metadata.find("_cue_offset") != metadata.end();
}

int Track::cueOffset()
{
    if (!isCueTrack())
        return 0;
    return metadata["_cue_offset"].toInt();
}

QString cueMetadataKey(int track, QString key)
{
    return QString("_cue_") + QString::number(track) + "_" + key;
}

QList<PTrack> Track::getCueTracks()
{
    QList<PTrack> tracks;
    if (!isCue())
        return tracks;
    QDir dir = QFileInfo(location).absoluteDir();
    int tracksno = metadata["_cue_tracks"].toInt();
    for (int i = 1; i <= tracksno; ++i) {
        PTrack track(new Track);
        auto set = [&](QString k, QString v) { track->metadata[k] = v; };
        for (QString& k : std::vector<QString>{"artist", "title", "_cue_offset"})
            set(k, metadata[cueMetadataKey(i, k)]);
        for (QString& k : std::vector<QString>{"album artist", "year", "album"})
            set(k, metadata[k]);
        set("track", QString::number(i));
        track->audioproperties = audioproperties;
        track->audioproperties.length = metadata[cueMetadataKey(i, "_cue_length")].toInt() / 1000;
        track->location = dir.absoluteFilePath(metadata[cueMetadataKey(i, "_cue_location")]);
        tracks.append(track);
    }
    return tracks;
}

void Track::setCueTracks(QList<PTrack> tracks)
{
    if (!isCue())
        return;
    metadata["_cue_tracks"] = QString::number(tracks.size());
    for (int i = 0; i < tracks.size(); ++i) {
        PTrack track = tracks[i];
        auto set = [&](QString k, QString v) { metadata[cueMetadataKey(i + 1, k)] = v; };
        for (QString& k : std::vector<QString>{"artist", "title", "_cue_offset"})
            set(k, track->metadata[k]);
        set("_cue_location", track->location);
        set("_cue_length", QString::number(track->audioproperties.length));
        //TODO save other audioproperties
    }
}

void Track::dump()
{
    qDebug() << "Path:" << location;
    for (QMap<QString, QString>::const_iterator it = metadata.begin(); it != metadata.end(); ++it)
        qDebug() << "    " << it.key().toUtf8() << "=" << it.value().toUtf8();
}

bool Track::isUrl() {
    return location.startsWith("http");
}

bool Track::updateDuration(int duration) {
    if (isUrl() && !isCueTrack()) {
        audioproperties.length = duration;
        return true;
    }
    return false;
}

bool Track::updateMetadata(const QString &title, const QString& audioFormat, int sampleRate) {
    if (isUrl() && !isCueTrack()) {
        if (metadata.value("title", "").isEmpty())
            metadata["title"] = title;
        metadata["audio-format"] = audioFormat;
        audioproperties.samplerate = sampleRate;
        return true;
    }
    return false;
}

void Track::updateAudioInfo(PTrack track)
{
    if (!track)
        return;
    qDebug() << "Updating cue" << track->path() << "with data from" << track->location;
    audioproperties = track->audioproperties;
    int offset = metadata[cueMetadataKey(metadata["_cue_tracks"].toInt(), "_cue_offset")].toInt();
    metadata[cueMetadataKey(metadata["_cue_tracks"].toInt(), "_cue_length")] = QString::number(audioproperties.length * 1000 - offset);
}

QString Track::cueTrackLocation()
{
    return metadata[cueMetadataKey(1, "_cue_location")];
}
