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

    void pause();
    void play();
    void play(qint64 offset);
    void seek(qint64 time);
    void stop();

public:
    AudioState state() const;
    qint64 currentTime() const;
    qint64 totalTime() const;
    void setCurrentSource(const QString& source);
    void enqueue(const QString& source);
    void clearQueue();

private slots:
    void slotStateChanged(Phonon::State newstate);
    void slotSeekableChanged(bool isSeekable);
    void aboutToFinishHandler();
    void currentSourceChangedHandler();
    void tickHandler(qint64 time);
    void slotFinished();

private:
    static AudioState audioState(Phonon::State state);

    Phonon::MediaObject* mediaObject_;
    Phonon::AudioOutput* audioOutput_;
    int offset_;
};
