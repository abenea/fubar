#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QObject>

class AudioOutput : public QObject
{
    Q_OBJECT
public:
    AudioOutput() {}
    virtual ~AudioOutput() {}

signals:
    void aboutToFinish();
    void currentSourceChanged();
    void tick(qint64 time);

public slots:
    virtual void setVolume(qreal newVolume) = 0;
    virtual void setTickInterval(qint32 newTickInterval) = 0;

    virtual void pause() = 0;
    virtual void play() = 0;
    virtual void seek(qint64 time) = 0;
    virtual void stop() = 0;

public:
    virtual bool paused() const = 0;
    virtual qint64 currentTime() const = 0;
    virtual qint64 totalTime() const = 0;
    virtual void setCurrentSource(const QString& source) = 0;
    virtual void enqueue(const QString& source) = 0;
    virtual void clearQueue() = 0;
};

#endif // AUDIOPLAYER_H
