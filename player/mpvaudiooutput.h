#pragma once
#include "audiooutput.h"
#include <mpv/qthelper.hpp>
#include <thread>

class MpvAudioOutput : public AudioOutput
{
    Q_OBJECT
public:
    MpvAudioOutput();
    ~MpvAudioOutput();

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

private:
    void setVolume();
    void setState(AudioState newState);
    void event_loop();
    void command(const QVariant& args);
    void set_property(const QString& name, const QVariant& v);
    void observe_property(const std::string& name, mpv_format format=MPV_FORMAT_STRING);

    mpv::qt::Handle handle_;
    AudioState state_;
    std::unique_ptr<std::thread> thread_;
    qreal volume_;
    qint64 seek_offset_;
};
