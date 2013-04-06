#ifndef MOCKAUDIOOUTPUT_H
#define MOCKAUDIOOUTPUT_H

#include "../../audiooutput.h"

class MockAudioOutput : public AudioOutput
{
    Q_OBJECT
public:
    MockAudioOutput() {}
    ~MockAudioOutput() {}

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
    QString currentSource() const;

    void triggerAboutToFinish();
    void triggerCurrentSourceChanged();

private:
    QString source_;
    QString nextSource_;
};

#endif // MOCKAUDIOOUTPUT_H
