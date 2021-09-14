#include "mprisplayer.h"
#include "audioplayer.h"
#include "mainwindow.h"
#include <QDBusConnection>
#include <QtDBus/QtDBus>


MPRISPlayer::MPRISPlayer(MainWindow* mainWindow, AudioPlayer* audioPlayer) : QDBusAbstractAdaptor(mainWindow), mainWindow_(mainWindow), audioplayer_(audioPlayer) {
    if (!QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.fubar")) {
        qDebug() << "Exposing DBUS service failed: " << QDBusConnection::sessionBus().lastError().message();
        return;
    }

    if (!QDBusConnection::sessionBus().registerObject("/org/mpris/MediaPlayer2", this, QDBusConnection::ExportAllSlots)) {
        qDebug() << "Exposing DBUS object failed: " << QDBusConnection::sessionBus().lastError().message();
        return;
    }
}

void MPRISPlayer::Play() {
    audioplayer_->play();
}

void MPRISPlayer::PlayPause() {
    audioplayer_->playPause();
}

void MPRISPlayer::Pause() {
    audioplayer_->pause();
}

void MPRISPlayer::Stop() {
    audioplayer_->stop();
}

void MPRISPlayer::Next() {
    audioplayer_->next();
}

void MPRISPlayer::Previous() {
    audioplayer_->prev();
}

void MPRISPlayer::ShowHide() {
    mainWindow_->showHide();
}
