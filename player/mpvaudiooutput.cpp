#include "mpvaudiooutput.h"
#include "audiostate.h"
#include <QDebug>
#include <mpv/client.h>

namespace mpv {
namespace qt {
static inline int command_variant2(mpv_handle *ctx, const QVariant &args) {
    node_builder node(args);
    return mpv_command_node(ctx, node.node(), 0);
}
}
}

namespace {
qint64 pos_to_qint64(const std::string &s) { return qint64(std::stof(s) * 1000); }
}

// TODO enable ytdl
MpvAudioOutput::MpvAudioOutput() : state_(AudioState::Stopped) {
    setlocale(LC_NUMERIC, "C");
    handle_ = mpv::qt::Handle::FromRawHandle(mpv_create());
    if (static_cast<mpv_handle *>(handle_) == nullptr)
        qDebug() << "Cannot mpv_create()";
    mpv::qt::set_option_variant(handle_, "vo", "null");
    mpv::qt::set_option_variant(handle_, "softvol", "yes");
    int r = mpv_initialize(handle_);
    if (r < 0) {
        qDebug() << "Failed to initialize mpv backend: " << mpv_error_string(r);
        //         raise Exception("");
    }
    thread_.reset(new std::thread([this] { event_loop(); }));
    observe_property("playback-time");
    observe_property("idle", MPV_FORMAT_FLAG);
    // TODO watch pause
}

MpvAudioOutput::~MpvAudioOutput() {
    command(QVariantList({"quit"}));
    thread_->join();
}

void MpvAudioOutput::setCurrentSource(const QString &source) {
    command(QVariantList({"loadfile", source}));
}

void MpvAudioOutput::clearQueue() { command(QVariantList({"playlist-clear"})); }

qint64 MpvAudioOutput::currentTime() const {
    auto r = get_property_variant(handle_, "playback-time");
    if (!r.isValid())
        return 0;
    return pos_to_qint64(r.toString().toStdString());
}

void MpvAudioOutput::enqueue(const QString &source) {
    command(QVariantList({"loadfile", source, "append"}));
}

void MpvAudioOutput::pause() {
    set_property("pause", true);
    setState(AudioState::Paused);
}

void MpvAudioOutput::play() {
    auto paused = get_property_variant(handle_, "pause");
    if (paused.isValid() and paused.toBool()) {
        set_property("pause", false);
        setState(AudioState::Playing);
    }
}

void MpvAudioOutput::play(qint64 offset) { Q_UNUSED(offset); }

void MpvAudioOutput::seek(qint64 time) {
    command(QVariantList({"seek", QString::number(time / 1000), "absolute"}));
}

void MpvAudioOutput::setVolume(qreal newVolume) {
    set_property("volume", static_cast<int>(newVolume * 100));
}

AudioState MpvAudioOutput::state() const { return state_; }

void MpvAudioOutput::stop() {
    command(QVariantList({"stop"}));
}

qint64 MpvAudioOutput::totalTime() const {
    auto d = get_property_variant(handle_, "duration");
    if (d == QVariant())
        return 0;
    return d.toInt();
}

void MpvAudioOutput::event_loop() {
    while (true) {
        auto event = mpv_wait_event(handle_, -1);
        //         qDebug() << "mpv event " << mpv_event_name(event->event_id);
        switch (event->event_id) {
        case MPV_EVENT_SHUTDOWN:
            return;
        case MPV_EVENT_QUEUE_OVERFLOW:
            qWarning() << "mpv queue overflow";
            break;
        case MPV_EVENT_FILE_LOADED:
            emit currentSourceChanged();
            break;
        case MPV_EVENT_END_FILE: {
            auto end_ev = reinterpret_cast<mpv_event_end_file *>(event->data);
            if (end_ev->reason == MPV_END_FILE_REASON_ERROR)
                qWarning() << "Ended file: " << mpv_error_string(end_ev->error);
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto prop = reinterpret_cast<mpv_event_property *>(event->data);
            if (prop->format != MPV_FORMAT_NONE && prop->data) {
                if (std::string(prop->name) == "playback-time") {
                    std::string pos(*(reinterpret_cast<char **>(prop->data)));
                    emit tick(pos_to_qint64(pos));
                } else if (std::string(prop->name) == "idle") {
                    int idle = *reinterpret_cast<int *>(prop->data);
                    if (idle) {
                        setState(AudioState::Stopped);
                        emit finished();
                    }
                    else
                        setState(AudioState::Playing);
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

void MpvAudioOutput::command(const QVariant &args) {
    int r = mpv::qt::command_variant2(handle_, args);
    if (r < 0)
        qDebug() << "Command failed: " << args << " " << mpv_error_string(r);
}

void MpvAudioOutput::set_property(const QString &name, const QVariant &v) {
    int r = set_property_variant(handle_, name, v);
    if (r < 0)
        qDebug() << "Failed to set property: " << name << " to " << v;
}

void MpvAudioOutput::observe_property(const std::string &name, mpv_format format) {
    auto r = mpv_observe_property(handle_, 0, name.c_str(), format);
    if (r < 0)
        qDebug() << "Failed mpv_observe_property " << name.c_str() << ": " << mpv_error_string(r);
}

void MpvAudioOutput::setState(AudioState newState) {
    state_ = newState;
    emit stateChanged(newState);
}
