#pragma once

#include "player/audiooutput.h"
#include <QStringList>

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

    QString nextSource() const { return nextSource_; }
    // Every source passed to setCurrentSource() or enqueue()
    const QStringList &received() const { return received_; }
    int stopCount() const { return stopCount_; }
    // If false, play() leaves the current source loading until triggerCurrentSourceChanged() or
    // triggerLoadFailed().
    void setLoadOnPlay(bool loadOnPlay) { loadOnPlay_ = loadOnPlay; }

    void triggerAboutToFinish();
    void triggerCurrentSourceChanged();
    void triggerLoadFailed(const QString &reason);
    void triggerFinished();
    void triggerTick(qint64 time) { emit tick(time); }

private:
    QString source_;
    QString nextSource_;
    QStringList received_;
    int stopCount_ = 0;
    bool loadOnPlay_ = true;
};
