#include "ui/mprisplayer.h"
#include "library/track.h"
#include "ui/audioplayer.h"
#include "ui/mainwindow.h"
#include <QDBusConnection>
#include <QtDBus/QtDBus>

MPRISPlayer::MPRISPlayer(MainWindow *mainWindow, AudioPlayer *audioPlayer)
    : QObject(mainWindow), mainWindow_(mainWindow), audioplayer_(audioPlayer) {
    rootAdaptor_ = new MPRISRootAdaptor(mainWindow, this);
    playerAdaptor_ = new MPRISPlayerAdaptor(mainWindow, audioPlayer, this);

    if (!QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.fubar")) {
        qDebug() << "Exposing DBUS service failed: "
                 << QDBusConnection::sessionBus().lastError().message();
    }

    if (!QDBusConnection::sessionBus().registerObject("/org/mpris/MediaPlayer2", this)) {
        qDebug() << "Exposing DBUS object failed: "
                 << QDBusConnection::sessionBus().lastError().message();
    }

    connect(audioplayer_, &AudioPlayer::trackPlaying, this, &MPRISPlayer::updateMetadata);
    connect(
        audioplayer_, &AudioPlayer::audioStateChanged, this, &MPRISPlayer::updatePlaybackStatus);
    connect(audioplayer_, &AudioPlayer::trackPositionChanged, this, &MPRISPlayer::onSeeked);
}

void MPRISPlayer::updateMetadata(PTrack track) {
    Q_UNUSED(track);
    QDBusMessage msg = QDBusMessage::createSignal(
        "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    QVariantMap changedProps;
    changedProps.insert("Metadata", QVariant::fromValue(playerAdaptor_->Metadata()));
    msg << "org.mpris.MediaPlayer2.Player" << changedProps << QStringList();
    QDBusConnection::sessionBus().send(msg);
}

void MPRISPlayer::updatePlaybackStatus(AudioState state) {
    Q_UNUSED(state);
    QDBusMessage msg = QDBusMessage::createSignal(
        "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    QVariantMap changedProps;
    changedProps.insert("PlaybackStatus", playerAdaptor_->PlaybackStatus());
    msg << "org.mpris.MediaPlayer2.Player" << changedProps << QStringList();
    QDBusConnection::sessionBus().send(msg);
}

void MPRISPlayer::onSeeked(qint64 position, bool userSeek) {
    if (userSeek) {
        emit playerAdaptor_->Seeked(position * 1000);
    }
}

MPRISRootAdaptor::MPRISRootAdaptor(MainWindow *mainWindow, MPRISPlayer *parent)
    : QDBusAbstractAdaptor(parent), mainWindow_(mainWindow) {}

void MPRISRootAdaptor::Raise() { mainWindow_->showHide(); }

void MPRISRootAdaptor::Quit() { mainWindow_->close(); }

MPRISPlayerAdaptor::MPRISPlayerAdaptor(MainWindow *mainWindow,
                                       AudioPlayer *audioPlayer,
                                       MPRISPlayer *parent)
    : QDBusAbstractAdaptor(parent), mainWindow_(mainWindow), audioplayer_(audioPlayer) {}

QString MPRISPlayerAdaptor::PlaybackStatus() const {
    switch (audioplayer_->state()) {
    case AudioState::Playing:
        return "Playing";
    case AudioState::Paused:
        return "Paused";
    case AudioState::Stopped:
    default:
        return "Stopped";
    }
}

QVariantMap MPRISPlayerAdaptor::Metadata() const {
    PTrack track = audioplayer_->getCurrentTrack();
    QVariantMap metadata;
    if (track) {
        metadata.insert("mpris:trackid",
                        QVariant::fromValue(QDBusObjectPath("/org/mpris/MediaPlayer2/Track/0")));
        metadata.insert("mpris:length", (qlonglong)track->audioproperties.length * 1000000);
        metadata.insert("xesam:title", track->metadata["title"]);
        metadata.insert("xesam:artist", QStringList() << track->metadata["artist"]);
        metadata.insert("xesam:album", track->metadata["album"]);
        metadata.insert("xesam:url", QUrl::fromLocalFile(track->location).toString());
    }
    return metadata;
}

double MPRISPlayerAdaptor::Volume() const { return audioplayer_->volume(); }

void MPRISPlayerAdaptor::SetVolume(double volume) {
    if (volume < 0)
        volume = 0;
    audioplayer_->setVolume(volume);
}

qlonglong MPRISPlayerAdaptor::Position() const { return audioplayer_->currentTime() * 1000; }

void MPRISPlayerAdaptor::Next() { audioplayer_->next(); }

void MPRISPlayerAdaptor::Previous() { audioplayer_->prev(); }

void MPRISPlayerAdaptor::Pause() { audioplayer_->pause(); }

void MPRISPlayerAdaptor::PlayPause() { audioplayer_->playPause(); }

void MPRISPlayerAdaptor::Stop() { audioplayer_->stop(); }

void MPRISPlayerAdaptor::Play() { audioplayer_->resume(); }

void MPRISPlayerAdaptor::Seek(qlonglong Offset) {
    audioplayer_->seek((audioplayer_->currentTime() * 1000 + Offset) / 1000);
}

void MPRISPlayerAdaptor::SetPosition(const QDBusObjectPath &TrackId, qlonglong Position) {
    Q_UNUSED(TrackId);
    audioplayer_->seek(Position / 1000);
}

void MPRISPlayerAdaptor::OpenUri(const QString &Uri) { Q_UNUSED(Uri); }
