#pragma once
#include "library/track_forward.h"
#include "player/audiostate.h"
#include "ui/playlistmodel_forward.h"
#include "ui/queue.h"
#include <QMap>
#include <QObject>
#include <functional>
#include <memory>
#include <set>

class Library;
class MainWindow;
class AudioOutput;

enum PlaybackOrder { Default, RepeatTrack, RepeatPlaylist, Random };

class AudioPlayer : public QObject {
    Q_OBJECT
public:
    AudioPlayer(Library *library,
                AudioOutput *audioOutput,
                bool testing = false,
                QObject *parent = 0);
    virtual ~AudioPlayer();

    static AudioPlayer *instance;

    Library *getLibrary() { return library_; }

    void setPlaybackOrder(PlaybackOrder playbackOrder);
    PlaybackOrder playbackOrder() { return playbackOrder_; }

    PTrack getCurrentTrack();
    PModel getPlayingPlaylistModel();
    QModelIndex getPlayingIndex();

    AudioState state();

    qint64 currentTime();
    void setVolume(qreal value);
    qreal volume() { return volume_; }
    void seek(qint64 pos);

    PModel createPlaylist(bool synced);
    // A read-only, sorted playlist not connected to the local library.
    PModel createReadOnlyPlaylist();
    void deletePlaylist(PModel model);

    void clearQueue();
    void enqueueTracks(PModel model, QModelIndexList tracks);

    void play(PModel playlistModel, const QModelIndex &index);

    void setLastPlayed(PModel playlistModel, const QModelIndex &index);
    std::pair<PModel, QModelIndex> getLastPlayed();

    void setMainWindow(MainWindow *mainWindow);

    bool isEnqueued(PModel playlistModel, QModelIndex index);

    // Maps track locations starting with prefix (e.g. "navidrome:") to a source the audio
    // output can play. Resolved sources are handed to the output only, never stored or logged.
    // An empty result makes the track unplayable: playbackError() reports `*error` (never null),
    // or "no source" if left empty.
    using SourceResolver = std::function<QString(const QString &location, QString *error)>;
    void registerSourceResolver(const QString &prefix, SourceResolver resolver);
    void unregisterSourceResolver(const QString &prefix);

signals:
    void playbackOrderChanged(PlaybackOrder newPlaybackOrder);

    void audioStateChanged(AudioState newState);
    void tick(qint64 pos);
    void trackPlaying(PTrack track);
    void stopped(qint64 /*ms*/ finalPosition, qint64 /*ms*/ trackLength);
    void trackPositionChanged(qint64 position, bool userSeek);
    // Playback stopped because `track` cannot be played. `reason` never contains a source URL.
    void playbackError(PTrack track, QString reason);

public slots:
    // Play from the beginning of the track
    void play();
    // Resume playing from the current position when paused or from the beginning when stopped
    void resume();
    void pause();
    void playPause();
    void stop();
    void next();
    void prev();

private slots:
    void slotAudioStateChanged(AudioState newState);
    void aboutToFinish();
    void currentSourceChanged();
    void slotTick(qint64 pos);
    void slotFinished();
    void configChanged(QString key, QVariant value);
    void durationChanged(double duration);
    void metadataChanged(QString title, QString audioFormat, int sampleRate);
    void slotLoadFailed(QString reason);

private:
    void setBuffering(PModel playlistModel, QModelIndex index, bool clearTrack = false);
    void setPlaying(PModel playlistModel, QModelIndex index);

    void playNext(int offset);
    void bufferTrack(PModel playlistModel, QModelIndex index);
    QModelIndex getNextModelIndex(PModel playlistModel, int offset);

    void writeSettings();
    void readSettings();

    // "" if a resolver matched but returned nothing.
    QString resolveSource(const QString &location, QString *error);
    QMap<QString, SourceResolver> sourceResolvers_;

    Library *library_;
    AudioOutput *audioOutput_;
    MainWindow *mainWindow_;

    // legal values in [0..1]
    qreal volume_;

    enum ReplayGainMode { None, Album, Track };
    QString replayGainToString(ReplayGainMode mode);
    ReplayGainMode replayGainFromString(QString str);
    ReplayGainMode replaygain_;
    qreal preamp_with_rg_;

    std::set<PModel> playlists_;

    Queue queue_;

    PlaybackOrder playbackOrder_;

    PModel playingModel_;
    QPersistentModelIndex playingIndex_;
    PTrack playingTrack_;

    // Hack used for selecting next track after deleting current track
    QPersistentModelIndex lastPlayedIndex_;

    PModel bufferingTrackPlaylist_;
    QPersistentModelIndex bufferingIndex_;
    PTrack bufferingTrack_;

    // Stopped: stop() was called and no play() since. Loading: play() handed a new source to
    // the output that has not been loaded yet.
    enum class SourceState { Stopped, Loading, Loaded };
    SourceState sourceState_ = SourceState::Loaded;

    bool lengthHack_;

    // If true, settings won't be read/written and shortcuts will be disabled.
    // useful for unit testing
    bool testing_;
};
