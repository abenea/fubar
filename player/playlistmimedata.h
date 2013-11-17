#pragma once
#include <QMimeData>
#include <QList>
#include "library/track_forward.h"

class PlaylistMimeData : public QMimeData
{
    Q_OBJECT
public:
    PlaylistMimeData(QList<PTrack> tracks);
    QList<PTrack> getTracks() const { return tracks_; }

private:
    QList<PTrack> tracks_;
};
