#include "player/playlist.h"

#include "library/track.h"
#include "library/track.pb.h"
#include <QDebug>
#include <QDir>
#include <QUrl>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tfilestream.h>

using namespace std;

void Playlist::addDirectory(const QString &path) {
    QDir dir(path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot);
    foreach (QFileInfo info, dir.entryInfoList()) {
        if (info.isFile()) {
            addFile(info.absoluteFilePath());
        } else {
            addDirectory(info.absoluteFilePath());
        }
    }
}

void Playlist::addFile(const QString &path) {
    TagLib::FileRef fileref;
    QByteArray encodedName = QFile::encodeName(path);
    TagLib::FileStream stream(encodedName.constData(), true);
    fileref = TagLib::FileRef(&stream, true);

    if (!fileref.isNull()) {
        shared_ptr<Track> track(new Track());
        track->location = path;
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
            track->audioproperties.length = audioProperties->lengthInSeconds();
            track->audioproperties.bitrate = audioProperties->bitrate();
            track->audioproperties.samplerate = audioProperties->sampleRate();
            track->audioproperties.channels = audioProperties->channels();
        }
        tracks.append(track);
    }
}

void Playlist::addM3u(const QString &path) {
    QFile m3u(path);
    if (!m3u.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    while (!m3u.atEnd()) {
        QByteArray line = m3u.readLine();
        if (!line.startsWith("#"))
            addFile(line.trimmed());
    }
}

void Playlist::addUrls(const QList<QUrl> &urls) {
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            QFileInfo info(url.toLocalFile());
            if (info.isDir())
                addDirectory(info.absoluteFilePath());
            else {
                if (info.suffix() == "m3u")
                    addM3u(info.absoluteFilePath());
                else
                    addFile(info.absoluteFilePath());
            }
        } else {
            qDebug() << "adding url " << url.toString();
            shared_ptr<Track> track(new Track());
            track->location = url.toString();
            track->audioproperties.length = 0;
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
    size_t len = plibrary.ByteSizeLong();
    bytes.resize(len);
    plibrary.SerializeToArray(bytes.data(), len);
}
