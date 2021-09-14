#pragma once
#include <QtDBus/QDBusAbstractAdaptor>

class AudioPlayer;
class MainWindow;

class MPRISPlayer: public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")

public:
    MPRISPlayer(MainWindow* mainWindow, AudioPlayer* audioPlayer);
public slots:
    Q_NOREPLY void Play();
    Q_NOREPLY void Pause();
    Q_NOREPLY void PlayPause();
    Q_NOREPLY void Stop();
    Q_NOREPLY void Next();
    Q_NOREPLY void Previous();
    Q_NOREPLY void ShowHide();
private:
    MainWindow* mainWindow_;
    AudioPlayer* audioplayer_;
};
