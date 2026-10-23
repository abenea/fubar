#include "player/tests/mockaudiooutput.h"
#include <QDebug>

void MockAudioOutput::triggerAboutToFinish() {
    source_.clear();
    emit aboutToFinish();
}

void MockAudioOutput::triggerCurrentSourceChanged() {
    if (!nextSource_.isEmpty()) {
        source_ = nextSource_;
        nextSource_.clear();
    }
    emit currentSourceChanged();
}

void MockAudioOutput::triggerLoadFailed(const QString &reason) { emit loadFailed(reason); }

void MockAudioOutput::triggerFinished() { emit finished(); }

void MockAudioOutput::setCurrentSource(const QString &source) {
    //     qDebug() << "MockAudioOutput::setCurrentSource" << source;
    received_.append(source);
    source_ = source;
}

QString MockAudioOutput::currentSource() const { return source_; }

void MockAudioOutput::clearQueue() { nextSource_.clear(); }

void MockAudioOutput::enqueue(const QString &source) {
    //     qDebug() << "MockAudioOutput::enqueue" << source;
    received_.append(source);
    nextSource_ = source;
}

void MockAudioOutput::play() {
    if (loadOnPlay_)
        emit currentSourceChanged();
}

void MockAudioOutput::stop() {
    ++stopCount_;
    source_.clear();
    nextSource_.clear();
}

void MockAudioOutput::pause() {}

AudioState MockAudioOutput::state() const { return AudioState::Unknown; }

void MockAudioOutput::seek(qint64 /*time*/) {}

qint64 MockAudioOutput::totalTime() const { return 0; }

qint64 MockAudioOutput::currentTime() const { return 0; }

void MockAudioOutput::setVolume(qreal /*newVolume*/) {}

void MockAudioOutput::setTickInterval(qint32 /*newTickInterval*/) {}
