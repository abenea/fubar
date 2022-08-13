#pragma once

#include "player/audiooutput.h"

class MockAudioOutput : public AudioOutput {
    Q_OBJECT
public:
    MockAudioOutput() {}
    ~MockAudioOutput() {}

public slots:
    void setVolume(qreal newVolume);
    void setTickInterval(qint32 newTickInterval);

    void pause();
    void play();
    void play(qint64 /*offset*/) {}
    void seek(qint64 time);
    void stop();

public:
    qint64 currentTime() const;
    qint64 totalTime() const;
    void setCurrentSource(const QString &source);
    void enqueue(const QString &source);
    void clearQueue();
    AudioState state() const;
    QString currentSource() const;

    void triggerAboutToFinish();
    void triggerCurrentSourceChanged();

private:
    QString source_;
    QString nextSource_;
};
