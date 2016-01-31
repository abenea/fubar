#include "playlist.h"
#include "library/track.pb.h"
#include "library/track.h"
#include <QDebug>
#include <QDir>
#include <QUrl>
#include <sstream>
#include <fstream>
#include <exception>
#include <stdexcept>
#include <taglib/fileref.h>
#include <taglib/tag.h>

using namespace std;

void Playlist::addDirectory(const QString &path) {
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

void Playlist::addFile(const QFileInfo &file) {
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

void Playlist::addUrls(const QList<QUrl>& urls) {
    for (const auto& url: urls) {
        if (url.isLocalFile()) {
            QFileInfo info(url.toLocalFile());
            if (info.isDir())
                addDirectory(info.absoluteFilePath());
            else
                addFile(info.absoluteFilePath());
        } else {
            shared_ptr<Track> track(new Track());
            track->location = url.toString();
            tracks.append(track);
        }
    }
}

void Playlist::deserialize(const QByteArray &bytes) {
    proto::Library plibrary;
    if (!plibrary.ParseFromArray(bytes.constData(), bytes.size())) {
        qDebug() << "Cannot parse proto::library for playlist";
        return;
    }
    for (int i = 0; i < plibrary.tracks_size(); ++i)
        tracks.append(shared_ptr<Track>(new Track(plibrary.tracks(i))));
}

void Playlist::serialize(QByteArray &bytes) const {
    proto::Library plibrary;
    for (PTrack track : tracks) {
        proto::Track *ptrack = plibrary.add_tracks();
        track->fillProtoTrack(*ptrack);
    }
    int len = plibrary.ByteSize();
    bytes.resize(len);
    plibrary.SerializeToArray(bytes.data(), len);
}
