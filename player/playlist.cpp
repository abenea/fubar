#include "playlist.h"
// #include "track.pb.h"
#include "library/track.h"
#include <QDebug>
#include <QDir>
#include <sstream>
#include <fstream>
#include <exception>
#include <stdexcept>
#include <taglib/fileref.h>
#include <taglib/tag.h>

using namespace std;

void Playlist::load(const char */*fileName*/)
{
/*    FILE *f = fopen(fileName, "rb");
    if (f == NULL) {
        throw runtime_error("Couldn't open " + string(fileName));
    }

    proto::Track ptrack;
    while (!feof(f))
    {
        if (Track::readProtoTrack(f, ptrack)) {
            tracks.append(shared_ptr<Track>(new Track(ptrack)));
        } else {
            if (!feof(f)) {
                ptrack.PrintDebugString();
                qDebug() << "bad parse";
            }
            break;
        }
    }
    fclose(f);*/
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

void Playlist::addFiles(const QStringList& files)
{
    foreach (QString file, files) {
        QFileInfo info(file);
        addFile(info);
    }
}

void Playlist::addFile(const QFileInfo& file)
{
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
