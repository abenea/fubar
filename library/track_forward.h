#pragma once

#include <QMetaType>
#include <memory>

class Track;
typedef std::shared_ptr<Track> PTrack;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(PTrack)
#endif
