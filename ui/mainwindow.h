#pragma once

#include <QKeySequence>
#include <QModelIndex>
#include <QSystemTrayIcon>
#include <QtWidgets/QMainWindow>
#include <boost/bimap.hpp>
#include <deque>
#include <memory>

#include "ui/audioplayer.h"
#include "ui/config.h"
#include "ui/playlistmodel_forward.h"
#include "ui/seekslider.h"
#include "ui/statusbar.h"

class PlaylistTab;
class Library;
class ConsoleWindow;
class QDockWidget;
class QPlainTextEdit;
class MPRISPlayer;

namespace Ui {
class MainWindowClass;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AudioPlayer &player, QWidget *parent = 0);
    ~MainWindow();

    static MainWindow *instance;

    bool cursorFollowsPlayback() { return cursorFollowsPlayback_; }

    Config &getConfig() { return config_; }

    // Returns the playlist that is the active tab
    PlaylistTab *getActivePlaylist();
    PModel getActivePlaylistModel();

    PlaylistTab *
    addPlaylist(PModel playlistTab, QString name = "Unnamed playlist", bool makeCurrent = true);

    void enqueueTracks(PModel playlistModel, QModelIndexList tracks);

    QModelIndex getRandomFilteredIndex(PModel playlistModel);
    QModelIndex getFilteredIndex(PModel playlistModel, QModelIndex current, int offset);
    QModelIndex getCurrentIndex(PModel playlistModel);

    bool isEnqueued(PlaylistTab *playlistTab, QModelIndex index);

    bool eventFilter(QObject *watched, QEvent *event);

public slots:
    void playbackOrderChanged(PlaybackOrder newPlaybackOrder);
    void volumeChanged(int value);
    void showHide();
    void removePlaylistTab(int index);
    void on_clearQueueAction_triggered();
    void lyricsUpdated(PTrack track);

protected:
    virtual void closeEvent(QCloseEvent *);

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
    void on_repeatPlaylistAction_triggered();

    void menuFileAboutToShow();
    void on_mainToolBar_actionTriggered(QAction *action);

    void on_showLyricsAction_triggered();
    void dockVisibilityChanged(bool visible);
    void consoleVisibilityChanged(bool visible);

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

    void configChanged(QString key, QVariant value);
    void newTabRequested();

    void increaseVolume();
    void decreaseVolume();

    void timeoutFired();

private:
    void readDockSettings(QDockWidget *dock, QString name);
    void writeDockSettings(QDockWidget *dock, QString name, bool isVisible);
    void setDockSize(QDockWidget *dock, QSize size);
    std::deque<std::function<void()>> dockHackFunctions_;

    PlaylistTab *getPlayingPlaylistTab();
    PlaylistTab *getPlaylistTab(PModel playlistModel);

    void setTrayIcon(bool playing);

    void setShortcuts();
    void addGlobalShortcut(QKeySequence shortcut, QObject *object, const char *slot, QString name);
    void addShortcut(QKeySequence shortcut,
                     const char *func,
                     Qt::ShortcutContext context = Qt::WindowShortcut);

    void writeSettings(bool lastPosition);
    void readSettings();

    std::unique_ptr<Ui::MainWindowClass> ui_;
    SeekSlider *seekSlider_;
    QSlider *volumeSlider_;

    StatusBar statusBar_;
    QSystemTrayIcon *trayIcon_;

    ConsoleWindow *console_;
    bool consoleEnabled_ = false;
    bool lyricsDockEnabled_ = false;
    Config config_;

    QDockWidget *lyricsDock_;
    QPlainTextEdit *lyricsWidget_;

    AudioPlayer &player_;

    MPRISPlayer *mprisPlayer_;

    boost::bimap<PModel, PlaylistTab *> playlistModels_;

    // If true, settings won't be read/written and shortcuts will be disabled.
    // useful for unit testing
    bool testing_;

    bool cursorFollowsPlayback_;

    bool saveTabs_;

    friend class PluginsPreferences;
};
