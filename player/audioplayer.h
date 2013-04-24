#pragma once

#include "audiostate.h"
#include "library/track_forward.h"
#include "playlistmodel_forward.h"
#include "queue.h"
#include <QObject>
#include <memory>
#include <set>

class Library;
class MainWindow;
class AudioOutput;

class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    AudioPlayer(Library* library, AudioOutput* audioOutput, bool testing = false, QObject* parent = 0);
    virtual ~AudioPlayer();

    static AudioPlayer *instance;

    void setRandom(bool random) { random_ = random; }
    bool random() { return random_; }

    PTrack getCurrentTrack();
    PModel getPlayingPlaylistModel();
    QModelIndex getPlayingIndex();

    qint64 currentTime();
    void setVolume(qreal value);
    qreal volume() { return volume_; }
    void seek(qint64 pos);

    PModel createPlaylist(bool synced);
    void deletePlaylist(PModel model);

    void clearQueue();
    void enqueueTracks(PModel model, QModelIndexList tracks);

    void play(PModel playlistModel, const QModelIndex &index);

    void setLastPlayed(PModel playlistModel, const QModelIndex &index);
    std::pair<PModel, QModelIndex> getLastPlayed();

    void setMainWindow(MainWindow* mainWindow);

    bool isEnqueued(PModel playlistModel, PTrack track);

signals:
    void randomChanged(bool random);

    void audioStateChanged(AudioState newState);
    void tick(qint64 pos);
    void trackPlaying(PTrack track);
    void stopped(qint64 /*ms*/ finalPosition, qint64 /*ms*/ trackLength);
    void trackPositionChanged(qint64 position, bool userSeek);

public slots:
    void play();
    void playPause();
    void stop();
    void next();
    void prev();

private slots:
    void slotAudioStateChanged(AudioState newState);
    void aboutToFinish();
    void currentSourceChanged();
    void slotTick(qint64 pos);

private:
    void setBuffering(PModel playlistModel, QModelIndex index, bool clearTrack = false);
    void setPlaying(PModel playlistModel, QModelIndex index);

    void playNext(int offset);
    void bufferTrack(PModel playlistModel, QModelIndex index);
    QModelIndex getNextModelIndex(PModel playlistModel, int offset);

    void writeSettings();
    void readSettings();

    Library* library_;
    AudioOutput* audioOutput_;
    MainWindow* mainWindow_;

    // legal values in [0..1]
    qreal volume_;

    std::set<PModel> playlists_;

    Queue queue_;

    bool random_;

    PModel playingModel_;
    QPersistentModelIndex playingIndex_;
    PTrack playingTrack_;

    // Hack used for selecting next track after deleting current track
    QPersistentModelIndex lastPlayedIndex_;

    PModel bufferingTrackPlaylist_;
    QPersistentModelIndex bufferingIndex_;
    PTrack bufferingTrack_;

    // If true, settings won't be read/written and shortcuts will be disabled.
    // useful for unit testing
    bool testing_;
};
