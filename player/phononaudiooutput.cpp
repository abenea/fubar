#include "phononaudiooutput.h"
#include "audiostate.h"
#include <phonon/Phonon/MediaObject>
#include <phonon/Phonon/AudioOutput>

PhononAudioOutput::PhononAudioOutput()
{
    audioOutput_ = new Phonon::AudioOutput(Phonon::MusicCategory, this);
    mediaObject_ = new Phonon::MediaObject(this);
    Phonon::createPath(mediaObject_, audioOutput_);
    QObject::connect(mediaObject_, SIGNAL(aboutToFinish()), this, SLOT(aboutToFinishHandler()));
    QObject::connect(mediaObject_, SIGNAL(currentSourceChanged(const Phonon::MediaSource &)), this, SLOT(currentSourceChangedHandler()));
    QObject::connect(mediaObject_, SIGNAL(tick(qint64)), this, SLOT(tickHandler(qint64)));
    QObject::connect(mediaObject_, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(slotStateChanged(Phonon::State)));
}

PhononAudioOutput::~PhononAudioOutput()
{
}

void PhononAudioOutput::play()
{
    mediaObject_->play();
}

void PhononAudioOutput::pause()
{
    mediaObject_->pause();
}

void PhononAudioOutput::stop()
{
    mediaObject_->stop();
}

void PhononAudioOutput::seek(qint64 time)
{
    mediaObject_->seek(time);
}

void PhononAudioOutput::enqueue(const QString& source)
{
    mediaObject_->enqueue(Phonon::MediaSource(source));
}

void PhononAudioOutput::clearQueue()
{
    mediaObject_->clearQueue();
}

void PhononAudioOutput::setCurrentSource(const QString& source)
{
    mediaObject_->setCurrentSource(Phonon::MediaSource(source));
}

void PhononAudioOutput::setVolume(qreal newVolume)
{
    audioOutput_->setVolume(newVolume);
}

AudioState PhononAudioOutput::state() const
{
    return audioState(mediaObject_->state());
}

qint64 PhononAudioOutput::currentTime() const
{
    return mediaObject_->currentTime();
}

qint64 PhononAudioOutput::totalTime() const
{
    return mediaObject_->totalTime();
}

void PhononAudioOutput::setTickInterval(qint32 newTickInterval)
{
    mediaObject_->setTickInterval(newTickInterval);
}

void PhononAudioOutput::aboutToFinishHandler()
{
    emit aboutToFinish();
}
void PhononAudioOutput::currentSourceChangedHandler()
{
    emit currentSourceChanged();
}

void PhononAudioOutput::tickHandler(qint64 time)
{
    emit tick(time);
}

void PhononAudioOutput::slotStateChanged(Phonon::State newstate)
{
    emit stateChanged(audioState(newstate));
}

AudioState PhononAudioOutput::audioState(Phonon::State state)
{
    switch (state) {
        case Phonon::PlayingState:
            return AudioState::Playing;
            break;
        case Phonon::StoppedState:
            return AudioState::Stopped;
            break;
        case Phonon::PausedState:
            return AudioState::Paused;
            break;
        case Phonon::BufferingState:
        case Phonon::LoadingState:
        case Phonon::ErrorState:
        default:
            return AudioState::Unknown;
    }
}

#include "phononaudiooutput.moc"