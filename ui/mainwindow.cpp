#include "ui/mainwindow.h"
#include "library/track.h"
#include "navidrome/navidromeclient.h"
#include "navidrome/navidromeconfig.h"
#include "navidrome/navidromelibrary.h"
#include "navidrome/navidromescrobbler.h"
#include "navidrome/navidrometrack.h"
#include "player/audiooutput.h"
#include "ui/configwindow.h"
#include "ui/consolewindow.h"
#include "ui/elidedlabel.h"
#include "ui/librarypreferencesdialog.h"
#include "ui/lyricsthread.h"
#include "ui/lyricsthreaddeleter.h"
#include "ui/mprisplayer.h"
#include "ui/navidromepreferences.h"
#include "ui/playlistfilter.h"
#include "ui/playlistmodel.h"
#include "ui/playlisttab.h"
#include "ui/pluginspreferences.h"
#include "ui/ui_mainwindow.h"
#include "util.h"
#include <QActionGroup>
#include <QDebug>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QShortcut>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <Qt>
#include <QtCore/qmath.h>
#include <kglobalaccel.h>
#include <kwindowsystem.h>
#include <kwindowsystem_version.h>
#include <kx11extras.h>
#include <netwm_def.h>

MainWindow *MainWindow::instance = 0;

namespace {

// Values of the mainwindow/tabsKinds setting
const char kPlaylistTab[] = "playlist";
const char kLibraryTab[] = "library";
const char kNavidromeTab[] = "navidrome";

const char kNavidromeTabName[] = "Navidrome";
constexpr int kStatusMaxWidth = 500;
constexpr int kNavidromeStartupCheckDelayMs = 3000;
constexpr int kNavidromeUpdateIntervalMs = 60 * 60 * 1000;
// How long "Navidrome: <n> tracks" stays
constexpr int kNavidromeSyncResultMs = 5000;

} // namespace

MainWindow::MainWindow(AudioPlayer &player, QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui::MainWindowClass),
      statusBar_(this),
      player_(player),
      cursorFollowsPlayback_(false),
      saveTabs_(false) {
    ui_->setupUi(this);
    // Not saving a pointer to this
    QActionGroup *playbackOrderGroup = new QActionGroup(this);
    playbackOrderGroup->addAction(ui_->defaultAction);
    playbackOrderGroup->addAction(ui_->randomAction);
    playbackOrderGroup->addAction(ui_->repeatTrackAction);
    playbackOrderGroup->addAction(ui_->repeatPlaylistAction);
    installEventFilter(this);

    player.setMainWindow(this);

    QObject::connect(&player_, SIGNAL(playbackOrderChanged(PlaybackOrder)), this,
                     SLOT(playbackOrderChanged(PlaybackOrder)));
    QObject::connect(&player_, SIGNAL(audioStateChanged(AudioState)), this,
                     SLOT(slotAudioStateChanged(AudioState)));
    QObject::connect(&player_, SIGNAL(trackPlaying(PTrack)), this, SLOT(updateUI(PTrack)));
    playbackOrderChanged(player.playbackOrder());

    QObject::connect(&player_, SIGNAL(tick(qint64)), this, SLOT(tick(qint64)));
    seekSlider_ = new SeekSlider(player_, this);
    volumeSlider_ = new QSlider(this);
    volumeSlider_->setOrientation(Qt::Horizontal);
    volumeSlider_->setMaximum(100);
    volumeSlider_->setMinimum(0);
    volumeSlider_->setValue(player_.volume() * 100);
    volumeSlider_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QObject::connect(volumeSlider_, SIGNAL(valueChanged(int)), this, SLOT(volumeChanged(int)));
    ui_->mainToolBar->addWidget(seekSlider_);
    ui_->mainToolBar->addWidget(volumeSlider_);

    trayIcon_ = new QSystemTrayIcon(this);
    setTrayIcon(false);
    connect(trayIcon_, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
            SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon_->show();

    setStatusBar(&statusBar_);
    QObject::connect(&statusBar_, SIGNAL(statusBarDoubleClicked()), this,
                     SLOT(statusBarDoubleClicked()));
    statusLabel_ = new ElidedLabel(kStatusMaxWidth, this);
    statusLabel_->setObjectName("StatusLabel");
    statusBar_.addPermanentWidget(statusLabel_);
    connect(&player_, &AudioPlayer::playbackError, this, &MainWindow::showPlaybackError);
    connect(&player_, &AudioPlayer::trackPlaying, this, [this]() {
        if (statusKind_ == StatusKind::PlaybackError)
            showStatus(QString());
    });

    lyricsDock_ = new QDockWidget("Lyrics", this);
    lyricsDock_->setObjectName("LyricsDock");
    lyricsWidget_ = new QPlainTextEdit(lyricsDock_);
    lyricsWidget_->setObjectName("LyricsWidget");
    lyricsWidget_->setReadOnly(true);
    lyricsDock_->setWidget(lyricsWidget_);
    QObject::connect(lyricsDock_, SIGNAL(visibilityChanged(bool)), this,
                     SLOT(dockVisibilityChanged(bool)));

    QObject::connect(ui_->playlistTabs, SIGNAL(tabCloseRequested(int)), this,
                     SLOT(removePlaylistTab(int)));
    QObject::connect(ui_->playlistTabs, SIGNAL(newTabRequested()), this, SLOT(newTabRequested()));

    QObject::connect(ui_->menu_File, SIGNAL(aboutToShow()), this, SLOT(menuFileAboutToShow()));

    setWindowIcon(QIcon(":/icon/logo22.png"));

    instance = this;

    console_ = new ConsoleWindow(this);
    console_->setObjectName("ConsoleWindow");
    QObject::connect(console_, SIGNAL(visibilityChanged(bool)), this,
                     SLOT(consoleVisibilityChanged(bool)));

    setUpNavidrome();

    readSettings();
    refreshNavidromeTabsAtStartup();

    config_.set("mainwindow.save_tabs", QVariant(saveTabs_));
    QObject::connect(&config_, SIGNAL(keySet(QString, QVariant)), this,
                     SLOT(configChanged(QString, QVariant)));

    setShortcuts();

    mprisPlayer_ = new MPRISPlayer(this, &player_);

    // Save tabs & window geometry every 5 minutes
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeoutFired()));
    timer->start(1000 * 60 * 5);
}

void MainWindow::newTabRequested() { on_newPlaylistAction_triggered(); }

void MainWindow::configChanged(QString key, QVariant value) {
    if (key == "mainwindow.save_tabs")
        saveTabs_ = value.toBool();
}

void MainWindow::addGlobalShortcut(QKeySequence shortcut,
                                   QObject *object,
                                   const char *slot,
                                   QString name) {
    QAction *action = new QAction(name, this);
    action->setObjectName(name);
    KGlobalAccel::setGlobalShortcut(action, shortcut);
    QObject::connect(action, SIGNAL(triggered()), object, slot);
}

void MainWindow::addShortcut(QKeySequence shortcut, const char *func, Qt::ShortcutContext context) {
    QShortcut *q = new QShortcut(shortcut, this, 0, 0, context);
    QObject::connect(q, SIGNAL(activated()), this, func);
}

void MainWindow::setShortcuts() {
    // Global shortcuts
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_W), this, SLOT(showHide()), "Show/Hide");
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_P), this, SLOT(showHide()), "Show/Hide");
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_X), &player_, SLOT(play()), "Play");
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_C), &player_, SLOT(playPause()),
                      "Play/Pause");
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_A), &player_, SLOT(prev()), "Prev");
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_Z), &player_, SLOT(next()), "Next");
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_V), &player_, SLOT(stop()), "Stop");
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_PageUp), this, SLOT(increaseVolume()),
                      "Increase volume");
    addGlobalShortcut(QKeySequence(Qt::META | Qt::Key_PageDown), this, SLOT(decreaseVolume()),
                      "Decrease volume");

    // App shortcuts
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_J), SLOT(focusFilter()));
    //     addShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), SLOT(removeActivePlaylist()));
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), SLOT(showHideConsole()),
                Qt::ApplicationShortcut);
}

MainWindow::~MainWindow() {
    player_.unregisterSourceResolver(navidrome::kLocationPrefix);
    // It refers to the client
    delete navidromePreferences_;
}

void MainWindow::setUpNavidrome() {
    QSettings settings;
    navidromeClient_.reset(new NavidromeClient);
    // Before the library and the scrobbler, which drop their data when the account changes
    navidromeClient_->setConfig(NavidromeConfig::load(settings));
    navidromeLibrary_.reset(new NavidromeLibrary(*navidromeClient_, QString()));
    navidromeScrobbler_.reset(new NavidromeScrobbler(*navidromeClient_, &player_));
    QTimer::singleShot(0, navidromeScrobbler_.get(), &NavidromeScrobbler::flushPending);
    player_.registerSourceResolver(
        navidrome::kLocationPrefix, [this](const QString &location, QString *error) {
            if (!navidromeClient_->isConfigured()) {
                *error = "Navidrome is not configured (Preferences → Navidrome…)";
                return QString();
            }
            QString songId = navidrome::songIdFromLocation(location);
            return navidromeClient_->streamUrl(songId).toString(QUrl::FullyEncoded);
        });

    // The models of the tabs follow it themselves (addNavidromeTab())
    NavidromeLibrary *library = navidromeLibrary_.get();
    connect(library, &NavidromeLibrary::tracksReset, this, [this]() {
        // A running sync was dropped
        if (statusKind_ == StatusKind::Info)
            showStatus(QString());
    });
    connect(library, &NavidromeLibrary::syncStarted, this,
            [this]() { showStatus("Navidrome: syncing…"); });
    connect(library, &NavidromeLibrary::syncFinished, this, &MainWindow::navidromeSyncFinished);
    connect(library, &NavidromeLibrary::syncFailed, this, [this](const QString &error) {
        showStatus("Navidrome: sync failed: " + error, StatusKind::SyncError);
    });

    navidromeUpdateTimer_ = new QTimer(this);
    navidromeUpdateTimer_->setObjectName("NavidromeUpdateTimer");
    navidromeUpdateTimer_->setInterval(kNavidromeUpdateIntervalMs);
    connect(navidromeUpdateTimer_, &QTimer::timeout, library, &NavidromeLibrary::checkForUpdates);

    ui_->refreshNavidromeLibraryAction->setEnabled(navidromeClient_->isConfigured());
    connect(navidromeClient_.get(), &NavidromeClient::connectionChanged, this, [this]() {
        ui_->refreshNavidromeLibraryAction->setEnabled(navidromeClient_->isConfigured());
    });
}

void MainWindow::refreshNavidromeTabsAtStartup() {
    if (navidromeTabs_.isEmpty() || !navidromeClient_->isConfigured())
        return;
    if (navidromeLibrary_->tracks().isEmpty()) {
        navidromeLibrary_->sync();
        return;
    }
    QTimer::singleShot(kNavidromeStartupCheckDelayMs, this, [this]() {
        if (!navidromeTabs_.isEmpty())
            navidromeLibrary_->checkForUpdates();
    });
}

void MainWindow::addNavidromeTab(const QString &name, bool makeCurrent) {
    PModel model = player_.createReadOnlyPlaylist();
    model->addTracks(navidromeLibrary_->tracks());
    NavidromeLibrary *library = navidromeLibrary_.get();
    connect(library, &NavidromeLibrary::tracksUpdated, model.get(), &PlaylistModel::updateTracks);
    connect(library, &NavidromeLibrary::tracksReset, model.get(), &PlaylistModel::updateTracks);
    navidromeTabs_.insert(addPlaylist(model, name, makeCurrent));
    if (!navidromeUpdateTimer_->isActive())
        navidromeUpdateTimer_->start();
}

void MainWindow::showStatus(const QString &text, StatusKind kind) {
    statusLabel_->setFullText(text);
    statusKind_ = kind;
    ++statusSerial_;
}

void MainWindow::navidromeSyncFinished(int trackCount) {
    showStatus("Navidrome: " + countText(trackCount, "track", "tracks"));
    int serial = statusSerial_;
    QTimer::singleShot(kNavidromeSyncResultMs, this, [this, serial]() {
        if (serial == statusSerial_)
            showStatus(QString());
    });
}

void MainWindow::showPlaybackError(PTrack track, QString reason) {
    if (!track)
        return;
    QString name = track->displayTitle();
    QString artist = track->metadata.value("artist");
    if (!artist.isEmpty() && !track->metadata.value("title").isEmpty())
        name = artist + " – " + name;
    // The name of a URL track can come from its query
    name = redactUrlCredentials(name);
    showStatus(QString("Cannot play %1: %2").arg(name, reason), StatusKind::PlaybackError);
    if (!navidrome::isNavidromeLocation(track->location) || !navidromeClient_->isConfigured())
        return;
    // mpv only knows that the stream did not play; the server may say why, e.g. a changed password
    int serial = statusSerial_;
    NavidromeClient::Params params{{"id", navidrome::songIdFromLocation(track->location)}};
    navidromeClient_->get("getSong", params, this,
                          [this, serial, name](const QJsonObject &, const QString &error) {
                              if (error.isEmpty() || serial != statusSerial_)
                                  return;
                              showStatus(QString("Cannot play %1: %2").arg(name, error),
                                         StatusKind::PlaybackError);
                          });
}

void MainWindow::showNavidromePreferences() {
    if (!navidromePreferences_) {
        navidromePreferences_ = new NavidromePreferences(*navidromeClient_, this);
        connect(navidromePreferences_, &NavidromePreferences::configSaved, this,
                &MainWindow::navidromeConfigSaved);
        connect(navidromePreferences_, &QObject::destroyed, this,
                [this]() { navidromeTabRequested_ = false; });
    }
    navidromePreferences_->show();
    navidromePreferences_->raise();
    navidromePreferences_->activateWindow();
}

void MainWindow::navidromeConfigSaved(bool accountChanged) {
    bool tabRequested = navidromeTabRequested_;
    navidromeTabRequested_ = false;
    if (statusKind_ != StatusKind::Info)
        showStatus(QString());
    if (!navidromeClient_->isConfigured())
        return;
    if (tabRequested || (accountChanged && navidromeTabs_.isEmpty()))
        addNavidromeTab(kNavidromeTabName, true);
    // An empty library can be a failed sync, e.g. with a wrong password for the same account
    if (accountChanged || (!navidromeTabs_.isEmpty() && navidromeLibrary_->tracks().isEmpty()))
        navidromeLibrary_->sync();
}

void MainWindow::increaseVolume() {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    volumeSlider_->event(&event);
}

void MainWindow::decreaseVolume() {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
    volumeSlider_->event(&event);
}

void MainWindow::playbackOrderChanged(PlaybackOrder newPlaybackOrder) {
    switch (newPlaybackOrder) {
    case PlaybackOrder::Random:
        ui_->randomAction->setChecked(true);
        break;
    case PlaybackOrder::RepeatTrack:
        ui_->repeatTrackAction->setChecked(true);
        break;
    case PlaybackOrder::RepeatPlaylist:
        ui_->repeatPlaylistAction->setChecked(true);
        break;
    case PlaybackOrder::Default:
    default:
        ui_->defaultAction->setChecked(true);
    }
}

void MainWindow::slotAudioStateChanged(AudioState newState) {
    setTrayIcon(newState == AudioState::Playing);
    if (newState == AudioState::Stopped) {
        updateUI(nullptr);
        statusBar_.clearMessage();
    }
}

void MainWindow::tick(qint64 /*pos*/) {
    PTrack track = player_.getCurrentTrack();
    if (track) {
        QString progress = formatDuration(player_.currentTime() / 1000) + " / " +
                           formatDuration(track->audioproperties.length);
        QString format = track->audioFormat();
        statusBar_.showMessage(format + " " +
                               (track->audioproperties.bitrate
                                    ? QString("%2kbps ").arg(track->audioproperties.bitrate)
                                    : "") +
                               QString("%3Hz  %4  %5")
                                   .arg(track->audioproperties.samplerate)
                                   .arg(QChar(164))
                                   .arg(progress));
    }
}

void MainWindow::timeoutFired() { writeSettings(false); }

void MainWindow::closeEvent(QCloseEvent *event) {
    player_.stop();
    writeSettings(true);
    QWidget::closeEvent(event);
}

void MainWindow::removeActivePlaylist() { removePlaylistTab(ui_->playlistTabs->currentIndex()); }

void MainWindow::menuFileAboutToShow() {
    PlaylistTab *current = getActivePlaylist();
    ui_->addFilesAction->setEnabled(current && current->isEditable());
    ui_->addDirectoryAction->setEnabled(current && current->isEditable());
}

void MainWindow::on_addDirectoryAction_triggered() {
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString directory =
        QFileDialog::getExistingDirectory(this, tr("Add directory"), QString(), options);
    if (directory.isEmpty())
        return;

    PlaylistTab *tab = getActivePlaylist();
    if (tab) {
        QList<QUrl> urls;
        urls.append(QUrl::fromLocalFile(directory));
        tab->addUrls(urls);
    }
}

void MainWindow::on_addFilesAction_triggered() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Select one or more files to open");
    if (files.isEmpty())
        return;

    PlaylistTab *tab = getActivePlaylist();
    if (tab) {
        QList<QUrl> urls;
        for (QString path : files) {
            urls.append(QUrl::fromLocalFile(path));
        }
        tab->addUrls(urls);
    }
}

PlaylistTab *MainWindow::addPlaylist(PModel playlistModel, QString name, bool makeCurrent) {
    if (!playlistModel)
        return nullptr;
    PlaylistTab *tab = new PlaylistTab(playlistModel);
    playlistModels_.insert({playlistModel, tab});
    ui_->playlistTabs->addTab(tab, name);
    if (makeCurrent)
        ui_->playlistTabs->setCurrentWidget(tab);
    return tab;
}

void MainWindow::removePlaylistTab(int index) {
    QWidget *widget = ui_->playlistTabs->widget(index);
    if (!widget)
        return;
    PlaylistTab *playlistTab = qobject_cast<PlaylistTab *>(widget);
    if (!playlistTab)
        return;

    ui_->playlistTabs->removeTab(index);
    PModel playlistModel = playlistModels_.right.at(playlistTab);
    playlistModels_.left.erase(playlistModel);
    if (navidromeTabs_.remove(playlistTab) && navidromeTabs_.isEmpty())
        navidromeUpdateTimer_->stop();
    player_.deletePlaylist(playlistModel);
    delete playlistTab;
}

void MainWindow::on_newLibraryViewAction_triggered() {
    addPlaylist(player_.createPlaylist(true), "All");
}

void MainWindow::on_newNavidromeLibraryViewAction_triggered() {
    if (!navidromeClient_->isConfigured()) {
        // The tab is created when the dialog saves a configuration
        showNavidromePreferences();
        navidromeTabRequested_ = true;
        return;
    }
    addNavidromeTab(kNavidromeTabName, true);
    if (navidromeLibrary_->tracks().isEmpty())
        navidromeLibrary_->sync();
    else
        navidromeLibrary_->checkForUpdates();
}

void MainWindow::on_refreshNavidromeLibraryAction_triggered() { navidromeLibrary_->sync(); }

void MainWindow::on_newPlaylistAction_triggered() { addPlaylist(player_.createPlaylist(false)); }

void MainWindow::on_quitAction_triggered() { close(); }

void MainWindow::on_libraryPreferencesAction_triggered() {
    if (player_.getLibrary()) {
        LibraryPreferencesDialog *widget =
            new LibraryPreferencesDialog(*player_.getLibrary(), this);
        widget->show();
    }
}

void MainWindow::on_navidromePreferencesAction_triggered() { showNavidromePreferences(); }

void MainWindow::on_pluginsAction_triggered() {
    PluginsPreferences *widget = new PluginsPreferences(this);
    widget->show();
}

void MainWindow::on_configAction_triggered() {
    ConfigWindow *cfg = new ConfigWindow(config_, this);
    cfg->show();
}

void MainWindow::on_clearQueueAction_triggered() { player_.clearQueue(); }

void MainWindow::on_cursorFollowsPlaybackAction_triggered() {
    cursorFollowsPlayback_ = !cursorFollowsPlayback_;
}

void MainWindow::on_randomAction_triggered() { player_.setPlaybackOrder(PlaybackOrder::Random); }

void MainWindow::on_defaultAction_triggered() { player_.setPlaybackOrder(PlaybackOrder::Default); }

void MainWindow::on_repeatTrackAction_triggered() {
    player_.setPlaybackOrder(PlaybackOrder::RepeatTrack);
}

void MainWindow::on_repeatPlaylistAction_triggered() {
    player_.setPlaybackOrder(PlaybackOrder::RepeatPlaylist);
}

void MainWindow::on_showLyricsAction_triggered() { lyricsDock_->setVisible(true); }

std::function<QString(const QString &)> dockSetting(QString &name) {
    return [&name](const QString &s) -> QString { return name + "/" + s; };
}

void MainWindow::writeDockSettings(QDockWidget *dock, QString name, bool isVisible) {
    QSettings settings;
    auto setting = dockSetting(name);
    settings.setValue(setting("dockarea"), dockWidgetArea(dock));
    settings.setValue(setting("floating"), dock->isFloating());
    settings.setValue(setting("visible"), isVisible);
    settings.setValue(setting("size"), dock->size());
    settings.setValue(setting("pos"), dock->pos());
}

void MainWindow::readDockSettings(QDockWidget *dock, QString name) {
    QSettings settings;
    auto setting = dockSetting(name);
    dock->setVisible(settings.value(setting("visible")).toBool());
    dock->setFloating(settings.value(setting("floating")).toBool());
    setDockSize(dock, settings.value(setting("size"), QSize(300, 300)).toSize());
    dock->move(settings.value(setting("pos"), QPoint(200, 200)).toPoint());
    addDockWidget(
        (Qt::DockWidgetArea)settings.value(setting("dockarea"), Qt::LeftDockWidgetArea).toInt(),
        dock);
}

void MainWindow::readSettings() {
    QSettings settings;
    readDockSettings(lyricsDock_, "lyrics");
    readDockSettings(console_, "console");

    restoreGeometry(settings.value("mainwindow/geometry").toByteArray());
    restoreState(settings.value("mainwindow/state").toByteArray());

    cursorFollowsPlayback_ =
        settings.value("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_).toBool();
    ui_->cursorFollowsPlaybackAction->setChecked(cursorFollowsPlayback_);

    saveTabs_ = settings.value("mainwindow/saveTabs", saveTabs_).toBool();
    if (!saveTabs_)
        on_newLibraryViewAction_triggered();
    else {
        QList<QVariant> names = settings.value("mainwindow/tabsNames").toList();
        QList<QVariant> data = settings.value("mainwindow/tabsData").toList();
        // Older versions wrote no kinds: tabs without data are library views
        QStringList kinds = settings.value("mainwindow/tabsKinds").toStringList();
        if (kinds.size() != data.size())
            kinds.clear();
        if (names.size() != data.size())
            qDebug() << "Tabs names and data have different sizes!";
        else {
            for (int i = 0; i < data.size(); ++i) {
                QString tabName = names.at(i).toString();
                if (data.at(i).isValid()) {
                    QByteArray tabData = data.at(i).toByteArray();
                    auto tab = addPlaylist(player_.createPlaylist(false), tabName, false);
                    tab->deserialize(tabData);
                } else if (kinds.value(i) == kNavidromeTab) {
                    addNavidromeTab(tabName, false);
                } else {
                    addPlaylist(player_.createPlaylist(true), tabName, false);
                }
            }
            int lastTab = settings.value("mainwindow/lastPlayingTab", 0).toInt();
            if (lastTab != -1)
                ui_->playlistTabs->setCurrentIndex(lastTab);
        }
    }

    int lastPlayingPosition = settings.value("mainwindow/lastPlayingPosition", -1).toInt();
    if (lastPlayingPosition != -1 && getActivePlaylist()) {
        QModelIndex index = getActivePlaylist()->getUnfilteredPosition(lastPlayingPosition);
        player_.setLastPlayed(getActivePlaylistModel(), index);
        getActivePlaylist()->updateCursorAndScroll(index);
    }
}

void MainWindow::writeSettings(bool lastPosition) {
    QSettings settings;
    settings.setValue("mainwindow/maximized", isMaximized());
    settings.setValue("mainwindow/geometry", saveGeometry());
    settings.setValue("mainwindow/state", saveState());
    writeDockSettings(lyricsDock_, "lyrics", lyricsDockEnabled_);
    writeDockSettings(console_, "console", consoleEnabled_);

    settings.setValue("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_);

    settings.setValue("mainwindow/saveTabs", saveTabs_);
    if (saveTabs_) {
        QList<QVariant> names;
        QList<QVariant> data;
        QStringList kinds;
        for (int i = 0; i < ui_->playlistTabs->count(); ++i) {
            names.append(ui_->playlistTabs->tabText(i));
            PlaylistTab *tab = dynamic_cast<PlaylistTab *>(ui_->playlistTabs->widget(i));
            if (tab->isEditable()) {
                QByteArray tabData;
                tab->serialize(tabData);
                data.append(tabData);
                kinds.append(kPlaylistTab);
            } else {
                // Restored as a library view by older versions
                data.append(QVariant());
                kinds.append(navidromeTabs_.contains(tab) ? kNavidromeTab : kLibraryTab);
            }
        }
        settings.setValue("mainwindow/tabsNames", QVariant(names));
        settings.setValue("mainwindow/tabsData", QVariant(data));
        settings.setValue("mainwindow/tabsKinds", kinds);
    }

    if (!lastPosition)
        return;
    int position = -1;
    int lastTab = -1;
    auto lastPlayed = player_.getLastPlayed();
    if (lastPlayed.first && lastPlayed.second.isValid()) {
        auto playlistTab = playlistModels_.left.at(lastPlayed.first);
        position = playlistTab->getUnfilteredPosition(lastPlayed.second);
        lastTab = ui_->playlistTabs->indexOf(playlistTab);
    }
    settings.setValue("mainwindow/lastPlayingTab", lastTab);
    settings.setValue("mainwindow/lastPlayingPosition", position);
}

void MainWindow::on_mainToolBar_actionTriggered(QAction *action) {
    if (action->text().toLower().contains("play")) {
        player_.play();
    } else if (action->text().toLower().contains("pause")) {
        player_.playPause();
    } else if (action->text().toLower().contains("stop")) {
        player_.stop();
    } else if (action->text().toLower().contains("prev")) {
        player_.prev();
    } else if (action->text().toLower().contains("next")) {
        player_.next();
    }
}

PlaylistTab *MainWindow::getActivePlaylist() {
    return dynamic_cast<PlaylistTab *>(ui_->playlistTabs->currentWidget());
}

PModel MainWindow::getActivePlaylistModel() {
    auto playlistTab = dynamic_cast<PlaylistTab *>(ui_->playlistTabs->currentWidget());
    if (!playlistTab)
        return nullptr;
    return playlistModels_.right.at(playlistTab);
}

void MainWindow::setTrayIcon(bool playing) {
    trayIcon_->setIcon(QIcon(playing ? ":/icon/logo22play.png" : ":/icon/logo22.png"));
}

void MainWindow::showHide() {
    bool hidden = !isVisible() || isMinimized();
    if (KWindowSystem::isPlatformX11()) {
        WId activeWindow = KX11Extras::activeWindow();
        hidden = hidden || (activeWindow != winId());
        if (hidden) {
            KX11Extras::setOnDesktop(winId(), KX11Extras::currentDesktop());
            setVisible(true);
            KX11Extras::forceActiveWindow(winId());
        } else {
            setVisible(false);
        }
    } else {
        // If the window is visible on another workspace this will hide the
        // window, so you have to press the shortcut twice :(
        setVisible(hidden);
    }
}

void MainWindow::statusBarDoubleClicked() {
    auto playlistTab = getPlayingPlaylistTab();
    if (playlistTab) {
        ui_->playlistTabs->setCurrentWidget(playlistTab);
        playlistTab->updateCursorAndScroll(AudioPlayer::instance->getPlayingIndex());
    }
}

void MainWindow::focusFilter() {
    if (getActivePlaylist())
        getActivePlaylist()->focusFilter();
}

void MainWindow::lyricsUpdated(PTrack track) {
    if (player_.getCurrentTrack() == track)
        lyricsWidget_->setPlainText(track->metadata.value("lyrics"));
}

void MainWindow::updateUI(PTrack track) {
    int seekMax = 0;
    QString title = "fubar";
    QString tray_tooltip;
    if (track) {
        if (cursorFollowsPlayback_) {
            auto playlistTab = getPlayingPlaylistTab();
            if (playlistTab)
                playlistTab->updateCursor(player_.getPlayingIndex());
        }
        QString track_info =
            (track->metadata["artist"].isEmpty() ? QString() : track->metadata["artist"] + " - ") +
            track->metadata["title"];
        title = track_info + "  [fubar]";
        tray_tooltip = track_info;
        seekMax = track->audioproperties.length;
        lyricsWidget_->setPlainText(track->metadata.value("lyrics"));
        if (track->metadata.value("lyrics").isEmpty()) {
            if (!track->metadata.value("artist").isEmpty() &&
                !track->metadata.value("title").isEmpty()) {
                LyricsThread *thread = new LyricsThread(track, this);
                new LyricsThreadDeleter(thread);
                thread->start();
            }
        }
    }
    trayIcon_->setToolTip(tray_tooltip);
    setWindowTitle(title);
    seekSlider_->setLimits(0, seekMax);
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        showHide();
    }
}

void MainWindow::volumeChanged(int value) { player_.setVolume(static_cast<qreal>(value) / 100); }

void MainWindow::enqueueTracks(PModel playlistModel, QModelIndexList tracks) {
    player_.enqueueTracks(playlistModel, tracks);
}

QModelIndex MainWindow::getCurrentIndex(PModel playlistModel) {
    auto playlistTab = getPlaylistTab(playlistModel);
    if (!playlistTab)
        return QModelIndex();
    return playlistTab->getCurrentIndex();
}

QModelIndex MainWindow::getFilteredIndex(PModel playlistModel, QModelIndex current, int offset) {
    auto playlistTab = getPlaylistTab(playlistModel);
    if (!playlistTab)
        return QModelIndex();
    return playlistTab->getFilteredIndex(current, offset);
}

QModelIndex MainWindow::getRandomFilteredIndex(PModel playlistModel) {
    auto playlistTab = getPlaylistTab(playlistModel);
    if (!playlistTab)
        return QModelIndex();
    return playlistTab->getRandomFilteredIndex();
}

PlaylistTab *MainWindow::getPlaylistTab(PModel playlistModel) {
    auto it = playlistModels_.left.find(playlistModel);
    return it != playlistModels_.left.end() ? it->second : nullptr;
}

PlaylistTab *MainWindow::getPlayingPlaylistTab() {
    return getPlaylistTab(player_.getPlayingPlaylistModel());
}

bool MainWindow::isEnqueued(PlaylistTab *playlistTab, QModelIndex index) {
    // The index is from the sortproxyfiltermodel
    return player_.isEnqueued(playlistModels_.right.at(playlistTab),
                              playlistTab->mapToSource(index));
}

void MainWindow::showHideConsole() {
    if (console_->isHidden()) {
        console_->show();
    } else {
        console_->hide();
    }
}

bool MainWindow::eventFilter(QObject * /*watched*/, QEvent *event) {
    return event->type() == QEvent::StatusTip;
}

void MainWindow::setDockSize(QDockWidget *dock, QSize size) {
    QSize oldMaxSize = dock->maximumSize();
    QSize oldMinSize = dock->minimumSize();

    if (size.width() >= 0) {
        if (dock->width() < size.width())
            dock->setMinimumWidth(size.width());
        else
            dock->setMaximumWidth(size.width());
    }
    if (size.height() >= 0) {
        if (dock->height() < size.height())
            dock->setMinimumHeight(size.height());
        else
            dock->setMaximumHeight(size.height());
    }
    auto func = [this, dock, oldMinSize, oldMaxSize]() -> void {
        dock->setMinimumSize(oldMinSize);
        dock->setMaximumSize(oldMaxSize);
        QTimer::singleShot(1, this, SLOT(restoreMaximizedState()));
    };
    dockHackFunctions_.push_back(func);
    QTimer::singleShot(1, this, SLOT(returnToOldMaxMinSizes()));
}

void MainWindow::returnToOldMaxMinSizes() {
    dockHackFunctions_.front()();
    dockHackFunctions_.pop_front();
}

void MainWindow::restoreMaximizedState() {
    QSettings qsettings;
    if (qsettings.value("mainwindow/maximized", true).toBool()) {
        showMaximized();
    }
}

void MainWindow::dockVisibilityChanged(bool visible) {
    if (isVisible())
        lyricsDockEnabled_ = visible;
}

void MainWindow::consoleVisibilityChanged(bool visible) {
    if (isVisible())
        consoleEnabled_ = visible;
}
