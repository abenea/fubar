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
             paths_.insert({track->path(), {{playlistTab, 1}}});
        } else {
            paths_[track->path()][playlistTab]++;
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
        if (!--it->second[playlistTab]) {
            it->second.erase(playlistTab);
            if (!it->second.size())
                paths_.erase(it);
        }
        if (!index.isValid())
            continue;
        return {playlistTab, index};
    }
    return {nullptr, QPersistentModelIndex(QModelIndex())};
}

void Queue::clear()
{
    while (!queue_.empty()) {
        queue_.pop();
    }
    paths_.clear();
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
