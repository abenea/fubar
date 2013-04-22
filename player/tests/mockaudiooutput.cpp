#include "mockaudiooutput.h"
#include <QDebug>

void MockAudioOutput::triggerAboutToFinish()
{
    source_.clear();
    emit aboutToFinish();
}

void MockAudioOutput::triggerCurrentSourceChanged()
{
    if (!nextSource_.isEmpty()) {
        source_ = nextSource_;
        nextSource_.clear();
    }
    emit currentSourceChanged();
}

void MockAudioOutput::setCurrentSource(const QString& source)
{
//     qDebug() << "MockAudioOutput::setCurrentSource" << source;
    source_ = source;
}

QString MockAudioOutput::currentSource() const
{
    return source_;
}

void MockAudioOutput::clearQueue()
{
    nextSource_.clear();
}

void MockAudioOutput::enqueue(const QString& source)
{
//     qDebug() << "MockAudioOutput::enqueue" << source;
    nextSource_ = source;
}

void MockAudioOutput::play()
{
    emit currentSourceChanged();
}

void MockAudioOutput::stop()
{
}

void MockAudioOutput::pause()
{
}

bool MockAudioOutput::paused() const
{
    return false;
}

void MockAudioOutput::seek(qint64 /*time*/)
{
}

qint64 MockAudioOutput::totalTime() const
{
    return 0;
}

qint64 MockAudioOutput::currentTime() const
{
    return 0;
}

void MockAudioOutput::setVolume(qreal /*newVolume*/)
{
}

void MockAudioOutput::setTickInterval(qint32 /*newTickInterval*/)
{
}

#include "mockaudiooutput.moc"
