#include "ui/mainwindow.h"
#include "library/track.h"
#include "player/audiooutput.h"
#include "ui/configwindow.h"
#include "ui/consolewindow.h"
#include "ui/librarypreferencesdialog.h"
#include "ui/lyricsthread.h"
#include "ui/lyricsthreaddeleter.h"
#include "ui/mprisplayer.h"
#include "ui/playlistfilter.h"
#include "ui/playlistmodel.h"
#include "ui/playlisttab.h"
#include "ui/pluginspreferences.h"
#include "ui/ui_mainwindow.h"
#include <QActionGroup>
#include <QDebug>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QShortcut>
#include <QTableView>
#include <QTextStream>
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

    readSettings();

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

MainWindow::~MainWindow() {}

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

QString msToPrettyTime(qint64 pos) {
    pos /= 1000;
    QString status_message;
    QTextStream(&status_message) << pos / 60 << ":" << qSetPadChar('0') << qSetFieldWidth(2)
                                 << pos % 60;
    return status_message;
}

void MainWindow::tick(qint64 /*pos*/) {
    PTrack track = player_.getCurrentTrack();
    if (track) {
        QString progress = msToPrettyTime(player_.currentTime()) + " / " +
                           msToPrettyTime(track->audioproperties.length * 1000);
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
    player_.deletePlaylist(playlistModel);
    delete playlistTab;
}

void MainWindow::on_newLibraryViewAction_triggered() {
    addPlaylist(player_.createPlaylist(true), "All");
}

void MainWindow::on_newPlaylistAction_triggered() { addPlaylist(player_.createPlaylist(false)); }

void MainWindow::on_quitAction_triggered() { close(); }

void MainWindow::on_libraryPreferencesAction_triggered() {
    if (player_.getLibrary()) {
        LibraryPreferencesDialog *widget =
            new LibraryPreferencesDialog(*player_.getLibrary(), this);
        widget->show();
    }
}

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
        if (names.size() != data.size())
            qDebug() << "Tabs names and data have different sizes!";
        else {
            for (int i = 0; i < data.size(); ++i) {
                QString tabName = names.at(i).toString();
                if (!data.at(i).isValid()) {
                    addPlaylist(player_.createPlaylist(true), tabName, false);
                } else {
                    QByteArray tabData = data.at(i).toByteArray();
                    auto tab = addPlaylist(player_.createPlaylist(false), tabName, false);
                    tab->deserialize(tabData);
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
        for (int i = 0; i < ui_->playlistTabs->count(); ++i) {
            names.append(ui_->playlistTabs->tabText(i));
            PlaylistTab *tab = dynamic_cast<PlaylistTab *>(ui_->playlistTabs->widget(i));
            if (tab->isEditable()) {
                QByteArray tabData;
                tab->serialize(tabData);
                data.append(tabData);
            } else {
                data.append(QVariant());
            }
        }
        settings.setValue("mainwindow/tabsNames", QVariant(names));
        settings.setValue("mainwindow/tabsData", QVariant(data));
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
    static bool consoleIsVisible = false;
    int currentDesktop = KX11Extras::currentDesktop();
    WId activeWindow = KX11Extras::activeWindow();
    if (!isVisible() || isMinimized() ||
        (activeWindow != winId() && activeWindow != console_->winId())) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
        KX11Extras::setOnDesktop(winId(), currentDesktop);
        if (consoleIsVisible)
            console_->show();
        setVisible(true);
        KX11Extras::forceActiveWindow(winId());
    } else {
        consoleIsVisible = console_->isVisible();
        console_->hide();
        setVisible(false);
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
#if KWINDOWSYSTEM_VERSION_MAJOR == 5
        KWindowSystem::setState(winId(), NET::Max);
#else
        KX11Extras::setState(winId(), NET::Max);
#endif
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
