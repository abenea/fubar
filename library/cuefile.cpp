#include "cuefile.h"
extern "C" {
#include <libcue.h>
}
#include <QDebug>
#include <stdexcept>
#include <functional>

CueFile::CueFile(QString path)
{
    f_ = fopen(path.toStdString().c_str(), "rt");
    if (!f_) {
        qDebug() << "Cannot open CUE" << path;
        throw std::runtime_error("");
    }
    cd_ = cue_parse_file(f_);
    if (!cd_) {
        qDebug() << "CUE file" << path << "is not valid";
        throw std::runtime_error("");
    }
}

CueFile::~CueFile()
{
    cd_delete(cd_);
    fclose(f_);
}

int CueFile::tracks()
{
    return cd_get_ntrack(cd_);
}

int cueLengthToMs(long length)
{
    // Cue track length is expressed in units of 1/75 seconds
    const int CUE_FPS = 75;
    return length * 1000 / CUE_FPS;
}

int CueFile::getLength(int trackno)
{
    Track* track = cd_get_track(cd_, trackno);
    return cueLengthToMs(track_get_length(track));
}

QString CueFile::getLocation(int trackno)
{
    Track* track = cd_get_track(cd_, trackno);
    return QString(track_get_filename(track));
}

std::function<void(char const*, char const*)> genSet(QMap<QString, QString>& m)
{
    return [&m](char const* k, char const* v) { m[k] = v ? QString::fromLatin1(v) : ""; };
}

QMap<QString, QString> CueFile::getMetadata()
{
    QMap<QString, QString> metadata;
    auto set = genSet(metadata);
    Cdtext* cdtext = cd_get_cdtext(cd_);
    if (cdtext) {
        set("album artist", cdtext_get(PTI_PERFORMER, cdtext));
        set("album", cdtext_get(PTI_TITLE, cdtext));
    }
    Rem* rem = cd_get_rem(cd_);
    if (rem) {
        set("year", rem_get(REM_DATE, rem));
    }
    return metadata;
}

QMap<QString, QString> CueFile::getMetadata(int trackno)
{
    QMap<QString, QString> metadata;
    auto set = genSet(metadata);
    Track* track = cd_get_track(cd_, trackno);
    if (track) {
//         metadata["track"] = QString::number(trackno);
        metadata["_cue_offset"] = QString::number(cueLengthToMs(track_get_start(track)));
        Cdtext* cdtext = track_get_cdtext(track);
        if (cdtext) {
            set("artist", cdtext_get(PTI_PERFORMER, cdtext));
            set("title", cdtext_get(PTI_TITLE, cdtext));
            if (metadata["artist"].isEmpty())
                metadata["artist"] = getMetadata().value("album artist");
        }
    }
    return metadata;
}
