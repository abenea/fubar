#include "audioplayer.h"
#include "phononaudiooutput.h"
#include "ui/mainwindow.h"
#include "library/library.h"
#include "library/track.h"
#include "playlistmodel.h"
#include <QtCore/qmath.h>
#include <QSettings>
#include <QDebug>

AudioPlayer *AudioPlayer::instance = nullptr;

AudioPlayer::AudioPlayer(Library* library, AudioOutput* audioOutput, bool testing, QObject* parent)
    : QObject(parent)
    , library_(library)
    , audioOutput_(audioOutput)
    , mainWindow_(nullptr)
    , random_(false)
    , testing_(testing)
{
    audioOutput_->setTickInterval(1000);
    QObject::connect(audioOutput_, SIGNAL(aboutToFinish()), this, SLOT(aboutToFinish()));
    QObject::connect(audioOutput_, SIGNAL(currentSourceChanged()), this, SLOT(currentSourceChanged()));
    QObject::connect(audioOutput_, SIGNAL(tick(qint64)), this, SLOT(slotTick(qint64)));
    QObject::connect(audioOutput_, SIGNAL(playingStateChanged(bool)), this, SIGNAL(playingStateChanged(bool)));
    // Not using Phono totalTimeChanged() signal because it returns 0 when used with enqueue()
    // TODO: report bug to phonon
//    QObject::connect(audioOutput_, SIGNAL(totalTimeChanged(qint64)), this, SLOT(totalTimeChanged(qint64)));
    readSettings();
    instance = this;
}

AudioPlayer::~AudioPlayer()
{
    writeSettings();
}

void AudioPlayer::setMainWindow(MainWindow* mainWindow)
{
    mainWindow_ = mainWindow;
}

void AudioPlayer::readSettings()
{
    if (testing_)
        return;
    QSettings settings;
    random_ = settings.value("mainwindow/random", random_).toBool();
    emit randomChanged(random_);
// TODO save/load last position
//     int lastPlayingPosition = settings.value("mainwindow/lastPlayingPosition", -1).toInt();
//     if (lastPlayingPosition != -1) {
//         mainWindow_->getActivePlaylistModel()->setCurrentPosition(lastPlayingPosition);
//         mainWindow_->getActivePlaylistModel()->updateCursorAndScroll();
//     }
    volume_ = settings.value("mainwindow/volume", 0).toReal();
    if (volume_ < 0 or volume_ > 1)
        volume_ = 0;
    setVolume(volume_);
}

void AudioPlayer::writeSettings()
{
    if (testing_)
        return;
    QSettings settings;
    settings.setValue("mainwindow/random", random_);
//     int position = -1;
//     QModelIndex index = playingIndex_.isValid() ? playingIndex_ : lastPlayedIndex_;
//     if (playingModel_ && playingModel_->playlist().synced && index.isValid())
//         position = index.row();
//     settings.setValue("mainwindow/lastPlayingPosition", position);
    settings.setValue("mainwindow/volume", volume_);
}

void AudioPlayer::aboutToFinish()
{
    if (!queue_.isEmpty()) {
        auto enqueued = queue_.peekTrack();
        if (enqueued.second.isValid()) {
            bufferTrack(enqueued.first, enqueued.second);
            return;
        }
    }
    if (playingModel_) {
        QModelIndex index = getNextModelIndex(playingModel_, 1);
        if (!index.isValid())
            return;
        bufferTrack(playingModel_, index);
    }
}

void AudioPlayer::setBuffering(PModel playlistModel, QModelIndex index, bool clearTrack)
{
    bufferingTrackPlaylist_ = playlistModel;
    bufferingIndex_ = QPersistentModelIndex(index);
    if (clearTrack)
        bufferingTrack_.reset();
    else if (index.isValid())
        bufferingTrack_ = index.data(TrackRole).value<std::shared_ptr<Track>>();
}

void AudioPlayer::setPlaying(PModel playlistModel, QModelIndex index)
{
    if (playlistModel == playingModel_)
        lastPlayedIndex_ = playingIndex_;
    playingModel_ = playlistModel;
    playingIndex_ = QPersistentModelIndex(index);
    if (index.isValid())
        playingTrack_ = index.data(TrackRole).value<std::shared_ptr<Track>>();
}

void AudioPlayer::bufferTrack(PModel playlistModel, QModelIndex index)
{
    setBuffering(playlistModel, index);
    if (!bufferingTrack_)
        return;
    audioOutput_->clearQueue();
    audioOutput_->enqueue(bufferingTrack_->location);
    qDebug() << "Enqueue " << bufferingTrack_->location;
}

void AudioPlayer::currentSourceChanged()
{
//     qDebug() << "currentSourceChanged() " << audioOutput_->currentTime() << " " << audioOutput_->totalTime();
    if (queue_.peeked())
        queue_.popPeekedTrack();
    if (bufferingTrackPlaylist_) {
        setPlaying(bufferingTrackPlaylist_, bufferingIndex_);
        setBuffering(PModel(), QModelIndex(), true);
    }
    if (playingTrack_) {
        setVolume(volume_);
        emit trackPlaying(playingTrack_);
        emit trackPositionChanged(0, true);
    } else {
        qDebug() << "Source changed to null track!";
    }
}

void AudioPlayer::slotTick(qint64 pos)
{
    emit trackPositionChanged(pos, false);
    emit tick(pos);
}

std::shared_ptr<Track> AudioPlayer::getCurrentTrack()
{
    return playingTrack_;
}

PModel AudioPlayer::getPlayingPlaylistModel()
{
    return playingModel_;
}

QModelIndex AudioPlayer::getPlayingIndex()
{
    return playingIndex_;
}

void AudioPlayer::play()
{
    playNext(0);
    if (playingTrack_) {
        emit trackPlaying(playingTrack_);
        emit trackPositionChanged(0, true);
    }
}

void AudioPlayer::playPause()
{
    if (audioOutput_->paused())
        audioOutput_->play();
    else
        audioOutput_->pause();
}

void AudioPlayer::next()
{
    if (!queue_.isEmpty()) {
        auto enqueued = queue_.popTrack();
        if (enqueued.second.isValid()) {
            playingModel_ = enqueued.first;
            play(playingModel_, enqueued.second);
            return;
        }
    }
    playNext(+1);
}

void AudioPlayer::prev()
{
    playNext(-1);
}

void AudioPlayer::stop()
{
    audioOutput_->stop();
    emit playingStateChanged(false);
    PTrack track = getCurrentTrack();
    if (track)
        emit stopped(audioOutput_->totalTime(), audioOutput_->currentTime());
}

std::shared_ptr<PlaylistModel> AudioPlayer::createPlaylist(bool synced)
{
    std::shared_ptr<PlaylistModel> model(new PlaylistModel(synced ? library_ : nullptr));
    playlists_.insert(model);
    return model;
}

void AudioPlayer::deletePlaylist(std::shared_ptr<PlaylistModel> model)
{
    queue_.removePlaylistModel(model);
    playlists_.erase(model);
    if (playingModel_ == model)
        setPlaying(PModel(), QModelIndex());
    if (bufferingTrackPlaylist_ == model)
        setBuffering(PModel(), QModelIndex());
}

void AudioPlayer::clearQueue()
{
//     bool peeked = queue.peeked();
    queue_.clear();
    // We can't buffer next song 'cause gstreamer is bugged
//     if (peeked) {
//         // buffer next song
//         playingModel_->enqueueNextTrack();
//     }
}

void AudioPlayer::enqueueTracks(PModel model, QModelIndexList tracks)
{
    queue_.pushTracks(model, tracks);
}

void AudioPlayer::play(PModel playlistModel, const QModelIndex& index)
{
    if (!index.isValid())
        return;
    setPlaying(playlistModel, index);
    setBuffering(PModel(), QModelIndex());
    if (!playingTrack_)
        return;

    audioOutput_->clearQueue();
    audioOutput_->setCurrentSource(playingTrack_->location);
    setVolume(volume_);
    audioOutput_->play();
}

void AudioPlayer::playNext(int offset)
{
    if (!playingModel_)
        playingModel_ = mainWindow_->getActivePlaylistModel();
    if (playingModel_)
        play(playingModel_, getNextModelIndex(playingModel_, offset));
}

QModelIndex AudioPlayer::getNextModelIndex(PModel playlistModel, int offset)
{
    if (offset && random_)
        return mainWindow_->getRandomFilteredIndex(playlistModel);
    // First play current index in model
    if (playingModel_ == playlistModel && playingIndex_.isValid())
        return mainWindow_->getFilteredIndex(playingModel_, playingIndex_, offset);
    // Try playing next track from lastplayed
    if (playingModel_ == playlistModel && lastPlayedIndex_.isValid())
        return mainWindow_->getFilteredIndex(playingModel_, lastPlayedIndex_, offset == -1 ? 0 : offset);
    // Try current index in model's view
    auto index = mainWindow_->getCurrentIndex(playlistModel);
    if (index.isValid())
        return index;
    // Try first element in playlist
    return mainWindow_->getFilteredIndex(playlistModel, QModelIndex(), 0);
}

bool AudioPlayer::isEnqueued(PModel playlistModel, PTrack track)
{
    return queue_.isQueued(playlistModel, track);
}

qint64 AudioPlayer::currentTime()
{
    return audioOutput_->currentTime();
}

void AudioPlayer::seek(qint64 pos)
{
    audioOutput_->seek(pos);
    emit trackPositionChanged(pos, true);
}

void AudioPlayer::setVolume(qreal value)
{
    volume_ = value;
    audioOutput_->setVolume(volume_);
    return;

    // TODO fix album formula and write a config UI
    PTrack track = getCurrentTrack();
    qreal volume = value;
    if (track) {
        // gain = 10 ^ ((rg + pream) / 20)
        QMap<QString, QString>::const_iterator it = track->metadata.find("REPLAYGAIN_TRACK_GAIN");
        qreal rg;
        if (it != track->metadata.end()) {
            QString track_rg = it.value();
            track_rg.chop(3);
            rg = track_rg.toDouble();
            qreal preamp = 10;
            rg = qPow(10, (preamp + rg) / 20);
            qDebug() << "Got track replaygain " << track_rg << " resulting in gain = " << rg;
        } else {
            rg = 1;
        }
        volume *= rg;
    }
//     qDebug() << "Setting volume to " << volume;
    audioOutput_->setVolume(volume);
}

#include "audioplayer.moc"
