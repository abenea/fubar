#pragma once

#include "player/audioplayer.h"
#include "player/playlistmodel_forward.h"
#include "ui/ui_mainwindow.h"
#include "config.h"
#include "statusbar.h"
#include "seekslider.h"
#include "pluginspreferences.h"
#include <QtGui/QMainWindow>
#include <QModelIndex>
#include <QKeySequence>
#include <QSystemTrayIcon>
#include <boost/bimap.hpp>
#include <memory>

class PlaylistTab;
class Library;
class ConsoleWindow;
class QDockWidget;
class QPlainTextEdit;

class MainWindow : public QMainWindow, private Ui::MainWindowClass
{
    Q_OBJECT
public:
    MainWindow(AudioPlayer& player, QWidget *parent = 0);
    ~MainWindow();

    static MainWindow *instance;

    bool cursorFollowsPlayback() { return cursorFollowsPlayback_; }

    Config& getConfig() { return config_; }

    // Returns the playlist that is the active tab
    PlaylistTab* getActivePlaylist();
    PModel getActivePlaylistModel();

    void addPlaylist(PModel playlistTab, QString name = "Unnamed playlist", bool makeCurrent = true);

    void enqueueTracks(PModel playlistModel, QModelIndexList tracks);

    QModelIndex getRandomFilteredIndex(PModel playlistModel);
    QModelIndex getFilteredIndex(PModel playlistModel, QModelIndex current, int offset);
    QModelIndex getCurrentIndex(PModel playlistModel);

    bool isEnqueued(PlaylistTab* playlistTab, QModelIndex index);

    bool eventFilter(QObject* watched, QEvent* event);

public slots:
    void playbackOrderChanged(PlaybackOrder newPlaybackOrder);
    void volumeChanged(int value);
    void showHide();
    void removePlaylistTab(int index);
    void on_clearQueueAction_triggered();

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
    void on_configAction_triggered();

    void on_cursorFollowsPlaybackAction_triggered();
    void on_randomAction_triggered();
    void on_defaultAction_triggered();
    void on_repeatTrackAction_triggered();

    void menuFileAboutToShow();
    void on_mainToolBar_actionTriggered(QAction* action);

    void on_showLyricsAction_triggered();

    void tick(qint64 pos);

    void statusBarDoubleClicked();
    void focusFilter();

    void updateUI(PTrack track);

    void slotAudioStateChanged(AudioState newState);
    void iconActivated(QSystemTrayIcon::ActivationReason reason);

    void removeActivePlaylist();

    void showHideConsole();

    void returnToOldMaxMinSizes();
    void restoreMaximizedState();

private:
    void setDockSize(QDockWidget *dock, QSize size);
    QSize oldMaxSize, oldMinSize;

    PlaylistTab* getPlayingPlaylistTab();
    PlaylistTab* getPlaylistTab(PModel playlistModel);

    void setTrayIcon(bool playing);

    void setShortcuts();
    void addGlobalShortcut(QKeySequence shortcut, QObject* object, const char* slot, QString name);
    void addShortcut(QKeySequence shortcut, const char* func, Qt::ShortcutContext context = Qt::WindowShortcut);

    void writeSettings();
    void readSettings();

    SeekSlider *seekSlider_;
    QSlider *volumeSlider_;

    StatusBar statusBar_;
    QSystemTrayIcon *trayIcon_;

    ConsoleWindow* console_;
    Config config_;

    QDockWidget* lyricsDock_;
    QPlainTextEdit* lyricsWidget_;

    AudioPlayer& player_;

    boost::bimap<PModel, PlaylistTab*> playlistModels_;

    // If true, settings won't be read/written and shortcuts will be disabled.
    // useful for unit testing
    bool testing_;

    bool cursorFollowsPlayback_;

    friend class PluginsPreferences;
};
