#include "queue.h"
#include "library/track.h"
#include "playlisttab.h"
#include <QDebug>
#include <boost/concept_check.hpp>

void Queue::pushTracks(PlaylistTab* playlistTab, QModelIndexList tracks)
{
    for (auto index : tracks) {
        auto track = index.data(TrackRole).value<PTrack>();
        auto it = paths_.find(track->path());
        if (it == paths_.end()) {
            paths_.insert(std::make_pair(track->path(), 1));
        } else {
            paths_[track->path()]++;
        }
        queue_.push(std::make_tuple(playlistTab, QPersistentModelIndex(index), track->path()));
    }
}

std::pair<PlaylistTab*, QPersistentModelIndex> Queue::popTrack()
{
    while (!queue_.empty()) {
        PlaylistTab* playlistTab;
        QPersistentModelIndex index;
        QString path;
        std::tie(playlistTab, index, path) = queue_.front();
        queue_.pop();
        auto it = paths_.find(path);
        if (!--it->second)
            paths_.erase(it);
        if (!index.isValid())
            continue;
        return std::make_pair(playlistTab, index);
    }
    return std::make_pair(nullptr, QPersistentModelIndex(QModelIndex()));
}

void Queue::clear()
{
    while (!queue_.empty()) {
        queue_.pop();
    }
    paths_.clear();
}

bool Queue::isQueued(std::shared_ptr<Track> track)
{
    return paths_.find(track->path()) != paths_.end();
}

bool Queue::isEmpty()
{
    return queue_.empty();
}
