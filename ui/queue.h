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
    Queue();

    void pushTracks(PlaylistTab* playlistTab, QModelIndexList tracks);
    std::pair<PlaylistTab*, QPersistentModelIndex> peekTrack();
    std::pair<PlaylistTab*, QPersistentModelIndex> popTrack();
    void popPeekedTrack();

    // A track was handed out but not deleted from the queue
    bool peeked();

    void clear();

    bool isEmpty();
    bool isQueued(PlaylistTab* playlistTab, std::shared_ptr<Track> track);

private:
    std::pair<PlaylistTab*, QPersistentModelIndex> getFirst(bool pop);
    std::pair<PlaylistTab*, QPersistentModelIndex> getNextTrack(bool pop);

    std::queue<std::tuple<PlaylistTab*, QPersistentModelIndex, QString>> queue_;
    std::map<QString, std::map<PlaylistTab*, int>> paths_;
    bool peeked_;
};

#endif // QUEUE_H
