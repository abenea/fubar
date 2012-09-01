#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "playlisttab.h"
#include "ui/ui_mainwindow.h"
#include "statusbar.h"
#include <QtGui/QMainWindow>
#include <QModelIndex>
#include <QKeySequence>
#include <phonon/Phonon/MediaObject>
#include <phonon/Phonon/AudioOutput>
#include <phonon/SeekSlider>
#include <phonon/VolumeSlider>


class PlaylistTab;
class Library;

class MainWindow : public QMainWindow, private Ui::MainWindowClass
{
    Q_OBJECT
public:
    MainWindow(Library& library, QWidget *parent = 0);
    ~MainWindow();

    Phonon::MediaObject *mediaObject;
    Phonon::AudioOutput *audioOutput;

    static MainWindow *instance;

    bool cursorFollowsPlayback() { return cursorFollowsPlayback_; }

    void setCurrentPlayingPlaylist(PlaylistTab *playlist);
    PlaylistTab* getCurrentPlaylist();
    PTrack getCurrentTrack();

signals:
    void trackPlaying(PTrack track);
    void stopped(qint64 /*ms*/ finalPosition, qint64 /*ms*/ trackLength);
    void trackPositionChanged(qint64 position, bool userSeek);

protected:
    virtual void closeEvent(QCloseEvent* );

private slots:
    void on_newPlaylistAction_triggered();
    void on_newLibraryViewAction_triggered();

    void on_addDirectoryAction_triggered();
    void on_addFilesAction_triggered();

    void on_preferencesAction_triggered();

    void on_cursorFollowsPlaybackAction_triggered();

    void on_mainToolBar_actionTriggered(QAction* action);

    void tick(qint64 pos);
    void aboutToFinish();
    void currentSourceChanged(const Phonon::MediaSource &);

    void play();
    void playPause();
    void stop();
    void next();
    void prev();
    void showHide();

    void statusBarDoubleClicked();

private:
    void setShortcuts();
    void addShortcut(QKeySequence shortcut, const char* func);

    // TODO: get notified about seeks
    Phonon::SeekSlider *seekSlider_;
    Phonon::VolumeSlider *volumeSlider_;

    StatusBar statusBar_;
    Library& library_;

    PlaylistTab *current();
    PlaylistTab *currentlyPlayingPlaylist_;

    void writeSettings();
    void readSettings();

    bool cursorFollowsPlayback_;
};

#endif // MAINWINDOW_H
