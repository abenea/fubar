#ifndef QUEUE_H
#define QUEUE_H

#include <QString>
#include <QModelIndex>
#include <tuple>
#include <utility>
#include <memory>
#include <map>
#include <queue>

class Track;
class PlaylistTab;

class Queue
{
public:
    void pushTracks(PlaylistTab* playlistTab, QModelIndexList tracks);
    std::pair<PlaylistTab*, QPersistentModelIndex> popTrack();
    void clear();

    bool isEmpty();
    bool isQueued(std::shared_ptr<Track> track);

private:
    std::queue<std::tuple<PlaylistTab*, QPersistentModelIndex, QString>> queue_;
    std::map<QString, int> paths_;
};

#endif // QUEUE_H
