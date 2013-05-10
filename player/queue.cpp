#include "queue.h"
#include "library/track.h"
#include "player/playlistmodel.h"
#include <QDebug>

Queue::Queue() : peeked_(false), enqueueOnlyOnce_(false)
{

}

void Queue::pushTracks(PModel playlistModel, QModelIndexList tracks)
{
    for (const auto& index : tracks) {
        auto track = index.data(TrackRole).value<PTrack>();
        auto it = paths_.find(track->path());
        auto queue_elem = std::make_tuple(playlistModel, QPersistentModelIndex(index), track->path());
        if (it == paths_.end()) {
            paths_.insert({track->path(), {{playlistModel, 1}}});
            queue_.push_back(queue_elem);
        } else {
            if (enqueueOnlyOnce_ && paths_[track->path()].find(playlistModel) != paths_[track->path()].end()) {
                paths_[track->path()].erase(playlistModel);
                for (auto it = queue_.begin(); it != queue_.end(); ++it)
                    if (*it == queue_elem) {
                        queue_.erase(it);
                        break;
                    }
            } else {
                paths_[track->path()][playlistModel]++;
                queue_.push_back(queue_elem);
            }
        }
    }
    std::vector<QPersistentModelIndex> persistent;
    for (const auto& index : tracks)
        persistent.push_back(QPersistentModelIndex(index));
    playlistModel->notifyQueueStatusChanged(persistent);
}

std::pair<PModel, QPersistentModelIndex> Queue::getFirst(bool pop)
{
    PModel playlistModel;
    QPersistentModelIndex index;
    QString path;
    std::tie(playlistModel, index, path) = queue_.front();
    if (!index.isValid() || (index.isValid() && pop)) {
        queue_.pop_front();
        auto it = paths_.find(path);
        if (!--it->second[playlistModel]) {
            it->second.erase(playlistModel);
            if (!it->second.size())
                paths_.erase(it);
        }
    }
    return {playlistModel, index};
}

std::pair<PModel, QPersistentModelIndex> Queue::getNextTrack(bool pop)
{
    while (!queue_.empty()) {
        auto item = getFirst(pop);
        if (!item.second.isValid())
            continue;
        return item;
    }
    return {nullptr, QPersistentModelIndex(QModelIndex())};
}

std::pair<PModel, QPersistentModelIndex> Queue::peekTrack()
{
    auto result = getNextTrack(false);
    peeked_ = result.second.isValid();
    return result;
}

std::pair<PModel, QPersistentModelIndex> Queue::popTrack()
{
    peeked_ = false;
    std::pair<PModel, QPersistentModelIndex> p = getNextTrack(true);
    if (p.first)
        p.first->notifyQueueStatusChanged({p.second});
    return p;
}

void Queue::popPeekedTrack()
{
    peeked_ = false;
    std::pair<PModel, QPersistentModelIndex> p = getFirst(true);
    p.first->notifyQueueStatusChanged({p.second});
}

bool Queue::isQueued(PModel playlistModel, PTrack track)
{
    auto it = paths_.find(track->path());
    if (it == paths_.end())
        return false;
    return it->second.find(playlistModel) != it->second.end();
}

bool Queue::isEmpty()
{
    return queue_.empty();
}

bool Queue::peeked()
{
    return peeked_;
}

void Queue::clear()
{
    peeked_ = false;
    std::map<PModel, std::vector<QPersistentModelIndex>> tracks;
    while (!queue_.empty()) {
        PModel p;
        QPersistentModelIndex index;
        QString path;
        std::tie(p, index, path) = queue_.front();
        auto it = tracks.find(p);
        if (it == tracks.end())
            tracks[p] = std::vector<QPersistentModelIndex>();
        tracks[p].push_back(index);
        queue_.pop_front();
    }
    paths_.clear();
    for (const auto& kv : tracks)
        kv.first->notifyQueueStatusChanged(kv.second);
}

void Queue::removePlaylistModel(PModel playlistModel)
{
    auto it = queue_.begin();
    while (it != queue_.end()) {
        PModel p;
        QPersistentModelIndex index;
        QString path;
        std::tie(p, index, path) = *it;
        if (p == playlistModel) {
            if (it == queue_.begin())
                peeked_ = false;
            it = queue_.erase(it);
            paths_.erase(path);
        } else
            ++it;
    }
}
