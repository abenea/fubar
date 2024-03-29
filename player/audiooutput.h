#pragma once

#include "player/audiostate.h"
#include <QObject>

class AudioOutput : public QObject {
    Q_OBJECT
public:
    AudioOutput() {}
    virtual ~AudioOutput() {}

signals:
    void aboutToFinish();
    void currentSourceChanged();
    // time in ms
    void tick(qint64 time);
    void stateChanged(AudioState state);
    void finished();
    void durationChanged(double);
    void metadataChanged(QString title, QString audioFormat, int sampleRate);

public slots:
    virtual void setVolume(qreal newVolume) = 0;

    virtual void pause() = 0;
    virtual void play() = 0;
    virtual void play(qint64 offset) = 0;
    virtual void seek(qint64 time) = 0;
    virtual void stop() = 0;

public:
    virtual AudioState state() const = 0;
    virtual qint64 currentTime() const = 0;
    virtual qint64 totalTime() const = 0;
    virtual void setCurrentSource(const QString &source) = 0;
    virtual void enqueue(const QString &source) = 0;
    virtual void clearQueue() = 0;
};
