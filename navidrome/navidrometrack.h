#pragma once

#include "library/track_forward.h"

#include <QList>
#include <QString>

class QJsonArray;
class QJsonObject;

namespace navidrome {

extern const QString kLocationPrefix;

bool isNavidromeLocation(const QString &location);
QString locationForSong(const QString &songId);
QString songIdFromLocation(const QString &location);
// The "id" of a song, album, artist or playlist, also when sent as a number.
QString idOf(const QJsonObject &object);
// nullptr for directories and songs without an id.
PTrack trackFromSong(const QJsonObject &song);
QList<PTrack> tracksFromSongs(const QJsonArray &songs);

} // namespace navidrome
