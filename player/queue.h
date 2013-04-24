#pragma once

#include "library/track_forward.h"
#include "playlistmodel_forward.h"
#include <QString>
#include <QModelIndex>
#include <tuple>
#include <utility>
#include <memory>
#include <map>
#include <deque>
#include <vector>

class Queue
{
public:
    Queue();

    void pushTracks(PModel playlistModel, QModelIndexList tracks);
    std::pair<PModel, QPersistentModelIndex> peekTrack();
    std::pair<PModel, QPersistentModelIndex> popTrack();
    void popPeekedTrack();

    // A track was handed out but not deleted from the queue
    bool peeked();

    void clear();

    bool isEmpty();
    bool isQueued(PModel playlistModel, std::shared_ptr<Track> track);

    void removePlaylistModel(PModel playlistModel);

private:
    std::pair<PModel, QPersistentModelIndex> getFirst(bool pop);
    std::pair<PModel, QPersistentModelIndex> getNextTrack(bool pop);

    std::deque<std::tuple<PModel, QPersistentModelIndex, QString>> queue_;
    // TODO this does not take into account that the same track could be multiple times in a playlist
    std::map<QString, std::map<PModel, int>> paths_;
    bool peeked_;
};
