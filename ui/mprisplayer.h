#pragma once

#include "library/track_forward.h"
#include "player/audiostate.h"
#include <QDBusAbstractAdaptor>
#include <QDBusVariant>

class AudioPlayer;
class MainWindow;

class MPRISPlayer : public QObject {
    Q_OBJECT
public:
    MPRISPlayer(MainWindow *mainWindow, AudioPlayer *audioPlayer);

private slots:
    void updateMetadata(PTrack track);
    void updatePlaybackStatus(AudioState state);
    void onSeeked(qint64 position, bool userSeek);

private:
    MainWindow *mainWindow_;
    AudioPlayer *audioplayer_;
    class MPRISRootAdaptor *rootAdaptor_;
    class MPRISPlayerAdaptor *playerAdaptor_;
};

class MPRISRootAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ CanQuit)
    Q_PROPERTY(bool CanRaise READ CanRaise)
    Q_PROPERTY(bool HasTrackList READ HasTrackList)
    Q_PROPERTY(QString Identity READ Identity)
    Q_PROPERTY(QString DesktopEntry READ DesktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ SupportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ SupportedMimeTypes)

public:
    MPRISRootAdaptor(MainWindow *mainWindow, MPRISPlayer *parent);

    bool CanQuit() const { return true; }
    bool CanRaise() const { return true; }
    bool HasTrackList() const { return false; }
    QString Identity() const { return "fubar"; }
    QString DesktopEntry() const { return "fubar"; }
    QStringList SupportedUriSchemes() const { return QStringList(); }
    QStringList SupportedMimeTypes() const { return QStringList(); }

public slots:
    void Raise();
    void Quit();

private:
    MainWindow *mainWindow_;
};

class MPRISPlayerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ PlaybackStatus)
    Q_PROPERTY(double Rate READ Rate WRITE SetRate)
    Q_PROPERTY(QVariantMap Metadata READ Metadata)
    Q_PROPERTY(double Volume READ Volume WRITE SetVolume)
    Q_PROPERTY(qlonglong Position READ Position)
    Q_PROPERTY(double MinimumRate READ MinimumRate)
    Q_PROPERTY(double MaximumRate READ MaximumRate)
    Q_PROPERTY(bool CanGoNext READ CanGoNext)
    Q_PROPERTY(bool CanGoPrevious READ CanGoPrevious)
    Q_PROPERTY(bool CanPlay READ CanPlay)
    Q_PROPERTY(bool CanPause READ CanPause)
    Q_PROPERTY(bool CanSeek READ CanSeek)
    Q_PROPERTY(bool CanControl READ CanControl)

public:
    MPRISPlayerAdaptor(MainWindow *mainWindow, AudioPlayer *audioPlayer, MPRISPlayer *parent);

    QString PlaybackStatus() const;
    double Rate() const { return 1.0; }
    void SetRate(double) {}
    QVariantMap Metadata() const;
    double Volume() const;
    void SetVolume(double volume);
    qlonglong Position() const;
    double MinimumRate() const { return 1.0; }
    double MaximumRate() const { return 1.0; }
    bool CanGoNext() const { return true; }
    bool CanGoPrevious() const { return true; }
    bool CanPlay() const { return true; }
    bool CanPause() const { return true; }
    bool CanSeek() const { return true; }
    bool CanControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong Offset);
    void SetPosition(const QDBusObjectPath &TrackId, qlonglong Position);
    void OpenUri(const QString &Uri);

signals:
    void Seeked(qlonglong Position);

private:
    MainWindow *mainWindow_;
    AudioPlayer *audioplayer_;
};
