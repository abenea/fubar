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

    bool enqueueOnlyOnce() { return enqueueOnlyOnce_; }
    void setEnqueueOnlyOnce(bool enabled) { enqueueOnlyOnce_ = enabled; }

    void pushTracks(PModel playlistModel, QModelIndexList tracks);
    std::pair<PModel, QPersistentModelIndex> peekTrack();
    std::pair<PModel, QPersistentModelIndex> popTrack();
    void popPeekedTrack();

    // A track was handed out but not deleted from the queue
    bool peeked();
    void unpeek() { peeked_ = false; }

    void clear();

    bool isEmpty();
    bool isQueued(PModel playlistModel, QModelIndex index);

    void removePlaylistModel(PModel playlistModel);

private:
    std::pair<PModel, QPersistentModelIndex> getFirst(bool pop);
    std::pair<PModel, QPersistentModelIndex> getNextTrack(bool pop);

    // TODO get rid of PModel
    std::deque<std::tuple<PModel, QPersistentModelIndex, QString>> queue_;
    std::map<QString, std::map<QPersistentModelIndex, int>> paths_;
    bool peeked_;
    bool enqueueOnlyOnce_;
};
