#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "playlisttab.h"
#include "libraryviewplaylist.h"
#include "ui/ui_mainwindow.h"
#include <boost/shared_ptr.hpp>
#include <QtGui/QMainWindow>
#include <QModelIndex>
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

public slots:
    void on_addDirectoryAction_triggered();
    void on_preferencesAction_triggered();
    void addView(LibraryViewPlaylist* view, const QString& name);

    void on_mainToolBar_actionTriggered(QAction* action);

protected:
    virtual void closeEvent(QCloseEvent* );

private:
    Phonon::SeekSlider *seekSlider_;
    Phonon::VolumeSlider *volumeSlider_;

    Library& library_;

    PlaylistTab *current();
    PlaylistTab *currentlyPlayingPlaylist_;

    void writeSettings();
    void readSettings();

private slots:
    void tick(qint64 pos);
};

#endif // MAINWINDOW_H
