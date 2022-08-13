#include "player/playlistmimedata.h"

PlaylistMimeData::PlaylistMimeData(QList<PTrack> tracks) : tracks_(tracks) {
    // Set a mimetype that PlaylistModel accepts
    setData("binary/playlist", QByteArray());
}
