#include "playlist.h"
#include "track.pb.h"
#include <QDebug>
#include <boost/scoped_array.hpp>
#include <fstream>
#include <exception>
#include <stdexcept>
#include <taglib/fileref.h>
#include <taglib/tag.h>

using namespace std;
using namespace boost;

// TODO this code is really bad, but I'm too lazy to change it
bool readTrack(FILE *f, proto::Track &track)
{
    int len;
    if (fread(&len, 4, 1, f) != 1)
        return false;
    // qDebug() << "read " << len << "size";
    if (len > (1 << 20)) {
        qDebug() << "OOps, huge len";
        return false;
    }
    scoped_array<char> tmp(new char[len]);
    if (fread(tmp.get(), len, 1, f) != 1) {
        qDebug() << "OOps, failed read for pb";
        return false;
    }
    return track.ParseFromArray(tmp.get(), len);
}


Track::Track(proto::Track ptrack)
{
    location = QString::fromUtf8(ptrack.location().c_str());
    for (int i = 0; i < ptrack.metadata().fields_size(); i++)
    {
        const proto::MetadataItem &field = ptrack.metadata().fields(i);
        metadata[QString::fromUtf8(field.name().c_str())] = QString::fromUtf8(field.value().c_str());
    }
}


void Playlist::load(const char *fileName)
{
    FILE *f = fopen(fileName, "rb");
    if (f == NULL) {
        throw runtime_error("Couldn't open " + string(fileName));
    }

    proto::Track ptrack;
    while (!feof(f))
    {
        if (readTrack(f, ptrack)) {
            tracks.append(shared_ptr<Track>(new Track(ptrack)));
        } else {
            if (!feof(f)) {
                ptrack.PrintDebugString();
                qDebug() << "bad parse";
            }
            break;
        }
    }
    fclose(f);
}

void Playlist::addDirectory(const QString &path)
{
    QDir dir(path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot);
    foreach (QFileInfo info, dir.entryInfoList()) {
        if (info.isFile()) {
            addFile(info);
        } else {
            addDirectory(info.filePath());
        }
    }
}

void Playlist::addFile(const QFileInfo& file)
{
    // qDebug() << "addFile " << file.filePath();
    TagLib::FileRef fileref;
    QByteArray encodedName = QFile::encodeName(file.filePath());
    fileref = TagLib::FileRef(encodedName.constData(), true);

    if (!fileref.isNull()) {
        shared_ptr<Track> track(new Track());
        track->location = file.filePath();
        if (TagLib::Tag *tag = fileref.tag()) {
            track->metadata["title"] = TStringToQString(tag->title());
            track->metadata["artist"] = TStringToQString(tag->artist());
            track->metadata["album"] = TStringToQString(tag->album());
            track->metadata["comment"] = TStringToQString(tag->comment());
            track->metadata["genre"] = TStringToQString(tag->genre());
            track->metadata["year"] = QString::number(tag->year());
            track->metadata["track"] = QString::number(tag->track());
        }
        if (TagLib::AudioProperties *audioProperties = fileref.audioProperties()) {
            track->audioproperties.length = audioProperties->length();
            track->audioproperties.bitrate = audioProperties->bitrate();
            track->audioproperties.samplerate = audioProperties->sampleRate();
            track->audioproperties.channels = audioProperties->channels();
        }
        tracks.append(track);
    }
}
