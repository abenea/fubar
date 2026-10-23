#pragma once

#include <QKeySequence>
#include <QModelIndex>
#include <QPointer>
#include <QSet>
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
class ElidedLabel;
class QDockWidget;
class QPlainTextEdit;
class QTimer;
class MPRISPlayer;
class NavidromeClient;
class NavidromeLibrary;
class NavidromePreferences;
class NavidromeScrobbler;

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
    void on_newNavidromeLibraryViewAction_triggered();
    void on_refreshNavidromeLibraryAction_triggered();
    void on_quitAction_triggered();

    void on_addDirectoryAction_triggered();
    void on_addFilesAction_triggered();

    void on_libraryPreferencesAction_triggered();
    void on_navidromePreferencesAction_triggered();
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

    void showPlaybackError(PTrack track, QString reason);
    void navidromeConfigSaved(bool accountChanged);
    void navidromeSyncFinished(int trackCount);

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

    void setUpNavidrome();
    // Syncs now if the cache is empty, else checks for updates soon. Run after readSettings().
    void refreshNavidromeTabsAtStartup();
    void addNavidromeTab(const QString &name, bool makeCurrent);
    void showNavidromePreferences();
    enum class StatusKind { Info, SyncError, PlaybackError };
    // Shown until replaced; a playback error is also cleared when a track starts playing.
    void showStatus(const QString &text, StatusKind kind = StatusKind::Info);

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

    std::unique_ptr<NavidromeClient> navidromeClient_;
    std::unique_ptr<NavidromeLibrary> navidromeLibrary_;
    std::unique_ptr<NavidromeScrobbler> navidromeScrobbler_;
    QPointer<NavidromePreferences> navidromePreferences_;
    // The open dialog was opened by File -> New Navidrome library view.
    bool navidromeTabRequested_ = false;
    // Tabs showing navidromeLibrary_; their models follow its changes.
    QSet<PlaylistTab *> navidromeTabs_;
    // Hourly freshness check while a Navidrome tab exists.
    QTimer *navidromeUpdateTimer_;
    // Navidrome syncs and playback errors
    ElidedLabel *statusLabel_;
    StatusKind statusKind_ = StatusKind::Info;
    // Incremented on every status change.
    int statusSerial_ = 0;

    boost::bimap<PModel, PlaylistTab *> playlistModels_;

    // If true, settings won't be read/written and shortcuts will be disabled.
    // useful for unit testing
    bool testing_;

    bool cursorFollowsPlayback_;

    bool saveTabs_;

    friend class PluginsPreferences;
};
