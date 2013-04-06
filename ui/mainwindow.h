#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "playlisttab.h"
#include "ui/ui_mainwindow.h"
#include "statusbar.h"
#include "seekslider.h"
#include "queue.h"
#include "pluginspreferences.h"
#include <QtGui/QMainWindow>
#include <QModelIndex>
#include <QKeySequence>
#include <QSystemTrayIcon>
#include <QPointer>
#include <boost/concept_check.hpp>

class PlaylistTab;
class Library;
class AudioOutput;

class MainWindow : public QMainWindow, private Ui::MainWindowClass
{
    Q_OBJECT
public:
    MainWindow(Library* library, AudioOutput* audioOutput, QWidget *parent = 0, bool testing = false);
    ~MainWindow();

    static MainWindow *instance;

    Queue queue;

    void enqueueTrack(PTrack track);
    void playTrack(PTrack track);

    bool cursorFollowsPlayback() { return cursorFollowsPlayback_; }
    bool random() { return random_; }

    void setCurrentPlayingPlaylist(PlaylistTab *playlist);

    // Returns the playlist that is the active tab
    PlaylistTab* getActivePlaylist();
    PTrack getCurrentTrack();

    void addPlaylist(PlaylistTab* playlistTab, QString name = "Unnamed playlist");

public slots:
    void volumeChanged();
    void play();
    void playPause();
    void stop();
    void next();
    void prev();
    void showHide();
    void removePlaylistTab(int index);
    void on_clearQueueAction_triggered();

signals:
    void trackPlaying(PTrack track);
    void stopped(qint64 /*ms*/ finalPosition, qint64 /*ms*/ trackLength);
    void trackPositionChanged(qint64 position, bool userSeek);

protected:
    virtual void closeEvent(QCloseEvent* );

private slots:
    void on_newPlaylistAction_triggered();
    void on_newLibraryViewAction_triggered();
    void on_quitAction_triggered();

    void on_addDirectoryAction_triggered();
    void on_addFilesAction_triggered();

    void on_libraryPreferencesAction_triggered();
    void on_pluginsAction_triggered();

    void on_cursorFollowsPlaybackAction_triggered();
    void on_randomAction_triggered();

    void menuFileAboutToShow();
    void on_mainToolBar_actionTriggered(QAction* action);

    void tick(qint64 pos);
    void aboutToFinish();
    void currentSourceChanged();

    qreal currentVolume();
    void setVolume(qreal value);
    void sliderMovedByUser(int pos);
    void statusBarDoubleClicked();
    void focusFilter();

    void updateUI(PTrack track);
    void totalTimeChanged(qint64 time);

    void iconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void repaintEnqueuedTrack(const QPersistentModelIndex& index);

    void setShortcuts();
    void addShortcut(QKeySequence shortcut, const char* func, QString name);

    void writeSettings();
    void readSettings();

    SeekSlider *seekSlider_;
    QSlider *volumeSlider_;

    StatusBar statusBar_;
    QSystemTrayIcon *trayIcon_;

    Library* library_;
    AudioOutput* audioOutput_;

    bool testing_;

    // TODO: QPointer<PlaylistTab> currentlyPlayingPlaylist_(); // sets a currentlyPlayingPlaylist_ to currently active if none
    QPointer<PlaylistTab> currentlyPlayingPlaylist_;
    QPointer<PlaylistTab> bufferingTrackPlaylist_;
    // PTrack currentPlayingTrack_; // Needed for cue sheets
    // Needed by lastfm when the current playing track is deleted from playlist before currentSourceChanged() is called
    PTrack bufferingTrack_;

    bool cursorFollowsPlayback_;
    bool random_;

    friend class PluginsPreferences;
};

#endif // MAINWINDOW_H
