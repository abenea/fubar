#pragma once

#include "audiooutput.h"
#include <phonon/Phonon/MediaObject>
#include <phonon/Phonon/AudioOutput>

class PhononAudioOutput : public AudioOutput
{
    Q_OBJECT
public:
    PhononAudioOutput();
    ~PhononAudioOutput();

public slots:
    void setVolume(qreal newVolume);
    void setTickInterval(qint32 newTickInterval);

    void pause();
    void play();
    void seek(qint64 time);
    void stop();

public:
    qint64 currentTime() const;
    qint64 totalTime() const;
    void setCurrentSource(const QString& source);
    void enqueue(const QString& source);
    void clearQueue();
    bool paused() const;

private slots:
    void slotStateChanged(Phonon::State newstate);
    void aboutToFinishHandler();
    void currentSourceChangedHandler();
    void tickHandler(qint64 time);

private:
    Phonon::MediaObject* mediaObject_;
    Phonon::AudioOutput* audioOutput_;
};
