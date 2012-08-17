#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "playlisttab.h"
#include "ui/ui_mainwindow.h"
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

    void setCurrentPlayingPlaylist(PlaylistTab *playlist);
    PlaylistTab* getCurrentPlaylist();

    bool cursorFollowsPlayback() { return cursorFollowsPlayback_; }

public slots:
    void on_newPlaylistAction_triggered();
    void on_newLibraryViewAction_triggered();

    void on_addDirectoryAction_triggered();
    void on_addFilesAction_triggered();

    void on_preferencesAction_triggered();

    void on_cursorFollowsPlaybackAction_triggered();

    void on_mainToolBar_actionTriggered(QAction* action);

    void AboutToFinish();
    void Play();
    void PlayPause();
    void Stop();
    void Next();
    void Prev();
    void ShowHide();

protected:
    virtual void closeEvent(QCloseEvent* );

private:
    void SetShortcuts();
    void AddShortcut(QKeySequence shortcut, const char* func);

    Phonon::SeekSlider *seekSlider_;
    Phonon::VolumeSlider *volumeSlider_;

    Library& library_;

    PlaylistTab *current();
    PlaylistTab *currentlyPlayingPlaylist_;

    void writeSettings();
    void readSettings();

    bool cursorFollowsPlayback_;

private slots:
    void tick(qint64 pos);
};

#endif // MAINWINDOW_H
