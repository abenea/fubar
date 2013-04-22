#ifndef QUEUE_H
#define QUEUE_H

#include <QString>
#include <QModelIndex>
#include <tuple>
#include <utility>
#include <memory>
#include <map>
#include <deque>
#include <vector>

class Track;
class PlaylistModel;

class Queue
{
public:
    Queue();

    void pushTracks(std::shared_ptr<PlaylistModel> playlistModel, QModelIndexList tracks);
    std::pair<std::shared_ptr<PlaylistModel>, QPersistentModelIndex> peekTrack();
    std::pair<std::shared_ptr<PlaylistModel>, QPersistentModelIndex> popTrack();
    void popPeekedTrack();

    // A track was handed out but not deleted from the queue
    bool peeked();

    void clear();

    bool isEmpty();
    bool isQueued(std::shared_ptr<PlaylistModel> playlistModel, std::shared_ptr<Track> track);

    void removePlaylistModel(std::shared_ptr<PlaylistModel> playlistModel);

private:
    std::pair<std::shared_ptr<PlaylistModel>, QPersistentModelIndex> getFirst(bool pop);
    std::pair<std::shared_ptr<PlaylistModel>, QPersistentModelIndex> getNextTrack(bool pop);

    std::deque<std::tuple<std::shared_ptr<PlaylistModel>, QPersistentModelIndex, QString>> queue_;
    // TODO this does not take into account that the same track could be multiple times in a playlist
    std::map<QString, std::map<std::shared_ptr<PlaylistModel>, int>> paths_;
    bool peeked_;
};

#endif // QUEUE_H
