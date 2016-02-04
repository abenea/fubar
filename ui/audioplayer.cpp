#include "audioplayer.h"
#include "player/phononaudiooutput.h"
#include "player/mpvaudiooutput.h"
#include "ui/mainwindow.h"
#include "library/library.h"
#include "library/track.h"
#include "playlistmodel.h"
#include <QtCore/qmath.h>
#include <QSettings>
#include <QDebug>
#include <cstdlib>

AudioPlayer *AudioPlayer::instance = nullptr;

AudioPlayer::AudioPlayer(Library* library, Backend backend, bool testing, QObject* parent)
    : QObject(parent)
    , library_(library)
    , mainWindow_(nullptr)
    , replaygain_(ReplayGainMode::None)
    , preamp_with_rg_(10)
    , playbackOrder_(PlaybackOrder::Default)
    , lengthHack_(false)
    , testing_(testing)
{
    if (backend == Backend::mpv)
        audioOutput_.reset(new MpvAudioOutput);
    else
        audioOutput_.reset(new PhononAudioOutput);
    QObject::connect(audioOutput_.get(), SIGNAL(aboutToFinish()), this, SLOT(aboutToFinish()));
    QObject::connect(audioOutput_.get(), SIGNAL(currentSourceChanged()), this, SLOT(currentSourceChanged()));
    QObject::connect(audioOutput_.get(), SIGNAL(tick(qint64)), this, SLOT(slotTick(qint64)));
    QObject::connect(audioOutput_.get(), SIGNAL(stateChanged(AudioState)), this, SLOT(slotAudioStateChanged(AudioState)));
    QObject::connect(audioOutput_.get(), SIGNAL(finished()), this, SLOT(slotFinished()));
    QObject::connect(audioOutput_.get(), SIGNAL(durationChanged(double)), this, SLOT(durationChanged(double)));
    QObject::connect(audioOutput_.get(), SIGNAL(metadataChanged(QString, QString, int)), this, SLOT(metadataChanged(QString, QString, int)));
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
    Config& config = mainWindow_->getConfig();
    config.set("playback.replaygain", QVariant(replayGainToString(replaygain_)));
    config.set("playback.replaygain.preamp_with_rg", QVariant(preamp_with_rg_));
    config.set("playback.enqueue.onlyOnce", QVariant(queue_.enqueueOnlyOnce()));
    config.set("library.length_hack", QVariant(lengthHack_));
    QObject::connect(&config, SIGNAL(keySet(QString,QVariant)), this, SLOT(configChanged(QString,QVariant)));
}

void AudioPlayer::configChanged(QString key, QVariant value)
{
    if (key == "playback.replaygain")
        replaygain_ = replayGainFromString(value.toString());
    else if (key == "playback.replaygain.preamp_with_rg")
        preamp_with_rg_ = value.toReal();
    else if (key == "playback.enqueue.onlyOnce")
        queue_.setEnqueueOnlyOnce(value.toBool());
    else if (key == "library.length_hack")
        lengthHack_ = value.toBool();
}

void AudioPlayer::readSettings()
{
    if (testing_)
        return;
    QSettings settings;
    replaygain_ = replayGainFromString(settings.value("playback/replaygain").toString());
    preamp_with_rg_ = settings.value("playback/replaygain.preamp_with_rg", preamp_with_rg_).toReal();
    playbackOrder_ = PlaybackOrder(settings.value("playback/order", static_cast<int>(playbackOrder_)).toInt());
    emit playbackOrderChanged(playbackOrder_);
    volume_ = settings.value("playback/volume", 0).toReal();
    if (volume_ < 0 or volume_ > 1)
        volume_ = 0;
    setVolume(volume_);
    queue_.setEnqueueOnlyOnce(settings.value("playback/enqueue.onlyOnce", false).toBool());
    lengthHack_ = settings.value("library/length_hack", lengthHack_).toBool();
}

void AudioPlayer::writeSettings()
{
    if (testing_)
        return;
    QSettings settings;
    settings.setValue("playback/order", playbackOrder_);
    settings.setValue("playback/volume", volume_);
    settings.setValue("playback/replaygain", replayGainToString(replaygain_));
    settings.setValue("playback/replaygain.preamp_with_rg", preamp_with_rg_);
    settings.setValue("playback/enqueue.onlyOnce", queue_.enqueueOnlyOnce());
    settings.setValue("library/length_hack", lengthHack_);
}

void AudioPlayer::aboutToFinish()
{
    if (!playingTrack_)
        return;
    audioOutput_->clearQueue();
    if (!queue_.isEmpty()) {
        auto enqueued = queue_.peekTrack();
        if (enqueued.second.isValid()) {
            bufferTrack(enqueued.first, enqueued.second);
            return;
        }
    }
    if (playingModel_) {
        QModelIndex index = getNextModelIndex(playingModel_, playbackOrder_ == PlaybackOrder::RepeatTrack ? 0 : 1);
        if (!index.isValid())
            return;
        bufferTrack(playingModel_, index);
    }
}

void AudioPlayer::setPlaybackOrder(PlaybackOrder playbackOrder) {
    playbackOrder_ = playbackOrder;
    aboutToFinish();
}

void AudioPlayer::setBuffering(PModel playlistModel, QModelIndex index, bool clearTrack)
{
    bufferingTrackPlaylist_ = playlistModel;
    bufferingIndex_ = QPersistentModelIndex(index);
    if (clearTrack)
        bufferingTrack_.reset();
    else if (index.isValid())
        bufferingTrack_ = index.data(TrackRole).value<PTrack>();
}

void AudioPlayer::setPlaying(PModel playlistModel, QModelIndex index)
{
    if (playlistModel == playingModel_)
        lastPlayedIndex_ = playingIndex_;
    playingModel_ = playlistModel;
    playingIndex_ = QPersistentModelIndex(index);
    if (index.isValid())
        playingTrack_ = index.data(TrackRole).value<PTrack>();
}

void AudioPlayer::bufferTrack(PModel playlistModel, QModelIndex index)
{
    setBuffering(playlistModel, index);
    if (!bufferingTrack_)
        return;
    if (!bufferingTrack_->isCueTrack()) {
        audioOutput_->clearQueue();
        audioOutput_->enqueue(bufferingTrack_->location);
        qDebug() << "Enqueue " << bufferingTrack_->location;
    }
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
    aboutToFinish();
}

void AudioPlayer::slotTick(qint64 pos)
{
    qint64 trackpos = playingTrack_->isCueTrack() ? pos - playingTrack_->cueOffset() : pos;
    emit trackPositionChanged(trackpos, false);
    emit tick(trackpos);
    // Length hack
    // If the pos > length, set length = position
    // This ugly uber lame hack exists because I'm too lazy to fix taglib
    // (taglib reports 0 length sometimes)
    if (lengthHack_ && !playingTrack_->isCueTrack() && playingTrack_->audioproperties.length + 1 < pos / 1000) {
        qDebug() << "Setting" << playingTrack_->path() << "length to " << pos / 1000 << "(was" << playingTrack_->audioproperties.length << ")";
        playingTrack_->audioproperties.length = pos / 1000;
        library_->dirtyHack(playingTrack_);
    }
    // Cue hack: if we're past the cue track position, go to next track
    if (playingTrack_ && playingTrack_->isCueTrack() && playingTrack_->audioproperties.length
        && playingTrack_->audioproperties.length * 1000 + 1000 <= trackpos) {
        aboutToFinish();
        slotFinished();
    }
}

PTrack AudioPlayer::getCurrentTrack()
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
}

void AudioPlayer::playPause()
{
    if (audioOutput_->state() == AudioState::Paused) {
        audioOutput_->play();
    }
    else if (audioOutput_->state() == AudioState::Playing) {
        audioOutput_->pause();
    } else {
        play();
    }
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
    setBuffering(PModel(), QModelIndex(), true);
    audioOutput_->stop();
}

PModel AudioPlayer::createPlaylist(bool synced)
{
    if (synced && !library_)
        return PModel();
    PModel model(new PlaylistModel(!synced));
    if (synced) {
        model->addTracks(library_->getTracks());
        QObject::connect(library_, SIGNAL(libraryChanged(LibraryEvent)), model.get(), SLOT(libraryChanged(LibraryEvent)));
        QObject::connect(library_, SIGNAL(libraryChanged(QList<PTrack>)), model.get(), SLOT(libraryChanged(QList<PTrack>)));
    }
    playlists_.insert(model);
    return model;
}

void AudioPlayer::deletePlaylist(PModel model)
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
    aboutToFinish();
}

void AudioPlayer::enqueueTracks(PModel model, QModelIndexList tracks)
{
    bool queue_was_empty = queue_.isEmpty();
    queue_.pushTracks(model, tracks);
    if (queue_was_empty)
        aboutToFinish();
}

void AudioPlayer::setLastPlayed(PModel playlistModel, const QModelIndex &index)
{
    playingModel_ = playlistModel;
    lastPlayedIndex_ = QPersistentModelIndex(index);
}

std::pair<PModel, QModelIndex> AudioPlayer::getLastPlayed()
{
    QModelIndex index = playingIndex_.isValid() ? playingIndex_ : lastPlayedIndex_;
    if (playingModel_ && index.isValid())
        return {playingModel_, index};
    return {PModel(), QModelIndex()};
}

void AudioPlayer::play(PModel playlistModel, const QModelIndex& index)
{
    if (!index.isValid())
        return;
    PTrack currentlyPlayingTrack = playingTrack_;
    setPlaying(playlistModel, index);
    setBuffering(PModel(), QModelIndex());
    if (!playingTrack_)
        return;

    if (currentlyPlayingTrack && audioOutput_->state() == AudioState::Playing && currentlyPlayingTrack->isCueTrack() && playingTrack_->isCueTrack()
        && currentlyPlayingTrack->location == playingTrack_->location) {
        // Playing in the same cue file
        audioOutput_->seek(playingTrack_->cueOffset());
        emit trackPlaying(playingTrack_);
        emit trackPositionChanged(0, true);
    } else {
        audioOutput_->clearQueue();
        audioOutput_->setCurrentSource(playingTrack_->location);
        setVolume(volume_);
        if (playingTrack_->isCueTrack()) {
            audioOutput_->play(playingTrack_->cueOffset());
        }
        else
            audioOutput_->play();
    }
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
    if (offset && playbackOrder_ == PlaybackOrder::Random)
        return mainWindow_->getRandomFilteredIndex(playlistModel);
    if (!offset && playbackOrder_ == PlaybackOrder::RepeatTrack && playingIndex_.isValid())
        return playingIndex_;
    // First play current index in model
    if (playingModel_ == playlistModel && playingIndex_.isValid()) {
        auto next = mainWindow_->getFilteredIndex(playingModel_, playingIndex_, offset);
        // If falling off the playlist end, play first track
        if (!next.isValid() && playbackOrder_ == PlaybackOrder::RepeatPlaylist)
            return mainWindow_->getFilteredIndex(playlistModel, QModelIndex(), 0);
        return next;
    }
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

bool AudioPlayer::isEnqueued(PModel playlistModel, QModelIndex index)
{
    return queue_.isQueued(playlistModel, index);
}

qint64 AudioPlayer::currentTime()
{
    return audioOutput_->currentTime() - playingTrack_->cueOffset();
}

void AudioPlayer::seek(qint64 pos)
{
    audioOutput_->seek(pos + playingTrack_->cueOffset());
    emit trackPositionChanged(pos, true);
}

void AudioPlayer::setVolume(qreal value)
{
    volume_ = value;
    qreal modified_volume = value;
    if (replaygain_ != ReplayGainMode::None) {
        PTrack track = getCurrentTrack();
        if (track) {
            // gain = 10 ^ ((rg + pream) / 20)
            auto id_tag = replaygain_ == ReplayGainMode::Album ? "REPLAYGAIN_ALBUM_GAIN" : "REPLAYGAIN_TRACK_GAIN";
            QMap<QString, QString>::const_iterator it = track->metadata.find(id_tag);
            qreal rg = 1;
            if (it != track->metadata.end()) {
                QString track_rg = it.value();
                track_rg.chop(3);
                rg = track_rg.toDouble();
                rg = qPow(10, (preamp_with_rg_ + rg) / 20);
                qDebug() << "Got" << replaygain_ <<  "replaygain " << track_rg << "resulting in gain =" << rg;
            }
            modified_volume *= rg;
        }
    }
    audioOutput_->setVolume(modified_volume);
}

void AudioPlayer::slotAudioStateChanged(AudioState newState)
{
    emit audioStateChanged(newState);
    if (newState == AudioState::Stopped)
        emit stopped(audioOutput_->totalTime(), audioOutput_->currentTime());
}

void AudioPlayer::slotFinished()
{
    if (!bufferingTrack_)
        return;
    if (bufferingTrack_->isCueTrack() && bufferingTrack_->location == playingTrack_->location &&
        bufferingTrack_->metadata["track"].toInt() == playingTrack_->metadata["track"].toInt() + 1) {
        currentSourceChanged();
        return;
    }
    if (queue_.peeked())
        queue_.unpeek();
    next();
}

void AudioPlayer::durationChanged(double duration) {
    if (!playingTrack_)
        return;

    playingTrack_->updateDuration(static_cast<int>(duration));
    playingModel_->libraryChanged(LibraryEvent(playingTrack_, LibraryEventType::MODIFY));
    // Update UI
    emit trackPlaying(playingTrack_);
}

void AudioPlayer::metadataChanged(QString title, QString audioFormat, int sampleRate) {
    if (!playingTrack_)
        return;

    playingTrack_->updateMetadata(title, audioFormat, sampleRate);
    playingModel_->libraryChanged(LibraryEvent(playingTrack_, LibraryEventType::MODIFY));
    // Update UI
    emit trackPlaying(playingTrack_);
}

AudioPlayer::ReplayGainMode AudioPlayer::replayGainFromString(QString str)
{
    str = str.toLower();
    if (str == "album")
        return ReplayGainMode::Album;
    if (str == "track")
        return ReplayGainMode::Track;
    return ReplayGainMode::None;
}

QString AudioPlayer::replayGainToString(AudioPlayer::ReplayGainMode mode)
{
    switch (mode) {
        case ReplayGainMode::Track:
            return "track";
        case ReplayGainMode::Album:
            return "album";
        case ReplayGainMode::None:
            ;
    }
    return "off";
}
