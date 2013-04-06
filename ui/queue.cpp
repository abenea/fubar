#include "queue.h"
#include "library/track.h"
#include "playlisttab.h"
#include <QDebug>

Queue::Queue() : peeked_(false)
{

}

void Queue::pushTracks(PlaylistTab* playlistTab, QModelIndexList tracks)
{
    for (auto index : tracks) {
        auto track = index.data(TrackRole).value<PTrack>();
        auto it = paths_.find(track->path());
        if (it == paths_.end()) {
            paths_.insert({track->path(), {{playlistTab, 1}}});
        } else {
            paths_[track->path()][playlistTab]++;
        }
        queue_.push_back(std::make_tuple(playlistTab, QPersistentModelIndex(index), track->path()));
    }
}


std::pair<PlaylistTab*, QPersistentModelIndex> Queue::getFirst(bool pop)
{
    PlaylistTab* playlistTab;
    QPersistentModelIndex index;
    QString path;
    std::tie(playlistTab, index, path) = queue_.front();
    if (!index.isValid() || (index.isValid() && pop)) {
        queue_.pop_front();
        auto it = paths_.find(path);
        if (!--it->second[playlistTab]) {
            it->second.erase(playlistTab);
            if (!it->second.size())
                paths_.erase(it);
        }
    }
    return {playlistTab, index};
}

std::pair<PlaylistTab*, QPersistentModelIndex> Queue::getNextTrack(bool pop)
{
    while (!queue_.empty()) {
        auto item = getFirst(pop);
        if (!item.second.isValid())
            continue;
        return item;
    }
    return {nullptr, QPersistentModelIndex(QModelIndex())};
}

std::pair<PlaylistTab*, QPersistentModelIndex> Queue::peekTrack()
{
    auto result = getNextTrack(false);
    peeked_ = result.second.isValid();
    return result;
}


std::pair<PlaylistTab*, QPersistentModelIndex> Queue::popTrack()
{
    peeked_ = false;
    return getNextTrack(true);
}

std::pair<PlaylistTab*, QPersistentModelIndex> Queue::popPeekedTrack()
{
    peeked_ = false;
    return getFirst(true);
}

bool Queue::isQueued(PlaylistTab* playlistTab, std::shared_ptr<Track> track)
{
    auto it = paths_.find(track->path());
    if (it == paths_.end())
        return false;
    return it->second.find(playlistTab) != it->second.end();
}

bool Queue::isEmpty()
{
    return queue_.empty();
}

bool Queue::peeked()
{
    return peeked_;
}

std::vector<QPersistentModelIndex> Queue::getTracksAndClear(PlaylistTab* playlist)
{
    peeked_ = false;
    std::vector<QPersistentModelIndex> result;
    while (!queue_.empty()) {
        PlaylistTab* p;
        QPersistentModelIndex index;
        QString path;
        std::tie(p, index, path) = queue_.front();
        if (p == playlist)
            result.push_back(index);
        queue_.pop_front();
    }
    paths_.clear();
    return result;
}

void Queue::removePlaylistTab(PlaylistTab* playlistTab)
{
    auto it = queue_.begin();
    while (it != queue_.end()) {
        PlaylistTab* p;
        QPersistentModelIndex index;
        QString path;
        std::tie(p, index, path) = *it;
        if (p == playlistTab) {
            if (it == queue_.begin())
                peeked_ = false;
            it = queue_.erase(it);
            paths_.erase(path);
        } else
            ++it;
    }
}
