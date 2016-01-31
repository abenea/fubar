#include "mainwindow.h"
#include "library/track.h"
#include "player/audiooutput.h"
#include "playlistmodel.h"
#include "playlisttab.h"
#include "playlistfilter.h"
#include "ui_mainwindow.h"
#include "librarypreferencesdialog.h"
#include "consolewindow.h"
#include "configwindow.h"
#include <Qt>
#include <QTableView>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <QUrl>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QVariant>
#include <QtCore/qmath.h>
#include <QTimer>
#include <kwindowsystem.h>
#include <kaction.h>
#include <netwm_def.h>

MainWindow *MainWindow::instance = 0;

MainWindow::MainWindow(AudioPlayer& player, QWidget *parent)
    : QMainWindow(parent)
    , statusBar_(this)
    , player_(player)
    , cursorFollowsPlayback_(false)
    , saveTabs_(false)
{
    setupUi(this);
    // Not saving a pointer to this
    QActionGroup* playbackOrderGroup = new QActionGroup(this);
    playbackOrderGroup->addAction(defaultAction);
    playbackOrderGroup->addAction(randomAction);
    playbackOrderGroup->addAction(repeatTrackAction);
    playbackOrderGroup->addAction(repeatPlaylistAction);
    installEventFilter(this);

    player.setMainWindow(this);

    QObject::connect(&player_, SIGNAL(playbackOrderChanged(PlaybackOrder)), this, SLOT(playbackOrderChanged(PlaybackOrder)));
    QObject::connect(&player_, SIGNAL(audioStateChanged(AudioState)), this, SLOT(slotAudioStateChanged(AudioState)));
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
    mainToolBar->addWidget(seekSlider_);
    mainToolBar->addWidget(volumeSlider_);

    trayIcon_ = new QSystemTrayIcon(this);
    setTrayIcon(false);
    connect(trayIcon_, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon_->show();

    setStatusBar(&statusBar_);
    QObject::connect(&statusBar_, SIGNAL(statusBarDoubleClicked()), this, SLOT(statusBarDoubleClicked()));

    lyricsDock_ = new QDockWidget("Lyrics", this);
    // Not setting an object name because we don't want save its state
    // since Qt doesn't restore it correctly
//     lyricsDock_->setObjectName("LyricsDock");
    lyricsWidget_ = new QPlainTextEdit(lyricsDock_);
//     lyricsWidget_->setObjectName("LyricsWidget");
    lyricsWidget_->setReadOnly(true);
    lyricsDock_->setWidget(lyricsWidget_);

//     playlistTabs->setTabsClosable(true);
    QObject::connect(playlistTabs, SIGNAL(tabCloseRequested(int)), this, SLOT(removePlaylistTab(int)));
    QObject::connect(playlistTabs, SIGNAL(newTabRequested()), this, SLOT(newTabRequested()));

    QObject::connect(menu_File, SIGNAL(aboutToShow()), this, SLOT(menuFileAboutToShow()));

    setWindowIcon(QIcon(":/icon/logo22.png"));

    instance = this;

    console_ = new ConsoleWindow(this);

    readSettings();

    config_.set("mainwindow.save_tabs", QVariant(saveTabs_));
    QObject::connect(&config_, SIGNAL(keySet(QString,QVariant)), this, SLOT(configChanged(QString,QVariant)));

    setShortcuts();
}

void MainWindow::newTabRequested()
{
    on_newPlaylistAction_triggered();
}

void MainWindow::configChanged(QString key, QVariant value)
{
    if (key == "mainwindow.save_tabs")
        saveTabs_ = value.toBool();
}

void MainWindow::addGlobalShortcut(QKeySequence shortcut, QObject* object, const char* slot, QString name)
{
    KAction* action = new KAction(name, this);
    action->setObjectName(name);
    action->setGlobalShortcut(KShortcut(shortcut), KAction::ActiveShortcut, KAction::Autoloading);
    QObject::connect(action, SIGNAL(triggered()), object, slot);

}

void MainWindow::addShortcut(QKeySequence shortcut, const char* func, Qt::ShortcutContext context)
{
    QShortcut* q = new QShortcut(shortcut, this, 0, 0, context);
    QObject::connect(q, SIGNAL(activated()), this, func);
}

void MainWindow::setShortcuts()
{
    // Global shortcuts
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_W), this, SLOT(showHide()), "Show/Hide");
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_P), this, SLOT(showHide()), "Show/Hide");
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_X), &player_, SLOT(play()), "Play");
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_C), &player_, SLOT(playPause()), "Play/Pause");
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_A), &player_, SLOT(prev()), "Prev");
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_Z), &player_, SLOT(next()), "Next");
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_V), &player_, SLOT(stop()), "Stop");
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_PageUp), this, SLOT(increaseVolume()), "Increase volume");
    addGlobalShortcut(QKeySequence(Qt::META + Qt::Key_PageDown), this, SLOT(decreaseVolume()), "Decrease volume");

    // App shortcuts
    addShortcut(QKeySequence(Qt::CTRL + Qt::Key_J), SLOT(focusFilter()));
//     addShortcut(QKeySequence(Qt::CTRL + Qt::Key_W), SLOT(removeActivePlaylist()));
    addShortcut(QKeySequence(Qt::CTRL + Qt::Key_D), SLOT(showHideConsole()), Qt::ApplicationShortcut);
}

MainWindow::~MainWindow()
{
}

void MainWindow::increaseVolume()
{
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    volumeSlider_->event(&event);
}

void MainWindow::decreaseVolume()
{
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
    volumeSlider_->event(&event);
}

void MainWindow::playbackOrderChanged(PlaybackOrder newPlaybackOrder)
{
    switch (newPlaybackOrder) {
        case PlaybackOrder::Random:
            randomAction->setChecked(true);
            break;
        case PlaybackOrder::RepeatTrack:
            repeatTrackAction->setChecked(true);
            break;
        case PlaybackOrder::RepeatPlaylist:
            repeatPlaylistAction->setChecked(true);
            break;
        case PlaybackOrder::Default:
        default:
            defaultAction->setChecked(true);
    }
}

void MainWindow::slotAudioStateChanged(AudioState newState)
{
    setTrayIcon(newState == AudioState::Playing);
    if (newState == AudioState::Stopped)  {
        updateUI(nullptr);
        statusBar_.clearMessage();
    }
}

QString msToPrettyTime(qint64 pos)
{
    pos /= 1000;
    QString status_message;
    QTextStream(&status_message) << pos / 60 << ":" << qSetPadChar('0') << qSetFieldWidth(2) << pos % 60;
    return status_message;
}

void MainWindow::tick(qint64 /*pos*/)
{
    PTrack track = player_.getCurrentTrack();
    if (track) {
        QString progress = msToPrettyTime(player_.currentTime()) + " / " + msToPrettyTime(track->audioproperties.length * 1000);
        QString format = QFileInfo(track->location).suffix().toUpper();
        statusBar_.showMessage(QString("%1 %2kbps %3Hz  %4  %5").arg(format).arg(track->audioproperties.bitrate).arg(track->audioproperties.samplerate).arg(QChar(164)).arg(progress));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    player_.stop();
    writeSettings();
    QWidget::closeEvent(event);
}

void MainWindow::removeActivePlaylist()
{
    removePlaylistTab(playlistTabs->currentIndex());
}

void MainWindow::menuFileAboutToShow()
{
    PlaylistTab* current = getActivePlaylist();
    addFilesAction->setEnabled(current && current->isEditable());
    addDirectoryAction->setEnabled(current && current->isEditable());
}

void MainWindow::on_addDirectoryAction_triggered()
{
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString directory = QFileDialog::getExistingDirectory(this, tr("Add directory"), QString(),
                                                          options);
    if (directory.isEmpty())
        return;

    PlaylistTab *tab = getActivePlaylist();
    if (tab) {
        QList<QUrl> urls;
        urls.append(QUrl::fromLocalFile(directory));
        tab->addUrls(urls);
    }
}

void MainWindow::on_addFilesAction_triggered()
{
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

PlaylistTab* MainWindow::addPlaylist(PModel playlistModel, QString name, bool makeCurrent)
{
    if (!playlistModel)
        return nullptr;
    PlaylistTab* tab = new PlaylistTab(playlistModel);
    playlistModels_.insert({playlistModel, tab});
    playlistTabs->addTab(tab, name);
    if (makeCurrent)
        playlistTabs->setCurrentWidget(tab);
    return tab;
}

void MainWindow::removePlaylistTab(int index)
{
    QWidget* widget = playlistTabs->widget(index);
    if (!widget)
        return;
    PlaylistTab* playlistTab = qobject_cast<PlaylistTab*>(widget);
    if (!playlistTab)
        return;

    playlistTabs->removeTab(index);
    PModel playlistModel = playlistModels_.right.at(playlistTab);
    playlistModels_.left.erase(playlistModel);
    player_.deletePlaylist(playlistModel);
    delete playlistTab;
}

void MainWindow::on_newLibraryViewAction_triggered()
{
    addPlaylist(player_.createPlaylist(true), "All");
}

void MainWindow::on_newPlaylistAction_triggered()
{
    addPlaylist(player_.createPlaylist(false));
}

void MainWindow::on_quitAction_triggered()
{
    close();
}

void MainWindow::on_libraryPreferencesAction_triggered()
{
    if (player_.getLibrary()) {
        LibraryPreferencesDialog* widget = new LibraryPreferencesDialog(*player_.getLibrary(), this);
        widget->show();
    }
}

void MainWindow::on_pluginsAction_triggered()
{
    PluginsPreferences* widget = new PluginsPreferences(this);
    widget->show();
}

void MainWindow::on_configAction_triggered()
{
    ConfigWindow* cfg = new ConfigWindow(config_, this);
    cfg->show();
}

void MainWindow::on_clearQueueAction_triggered()
{
    player_.clearQueue();
}

void MainWindow::on_cursorFollowsPlaybackAction_triggered()
{
    cursorFollowsPlayback_ = !cursorFollowsPlayback_;
}

void MainWindow::on_randomAction_triggered()
{
    player_.setPlaybackOrder(PlaybackOrder::Random);
}

void MainWindow::on_defaultAction_triggered()
{
    player_.setPlaybackOrder(PlaybackOrder::Default);
}

void MainWindow::on_repeatTrackAction_triggered()
{
    player_.setPlaybackOrder(PlaybackOrder::RepeatTrack);
}

void MainWindow::on_repeatPlaylistAction_triggered()
{
    player_.setPlaybackOrder(PlaybackOrder::RepeatPlaylist);
}

void MainWindow::on_showLyricsAction_triggered()
{
    lyricsDock_->setVisible(true);
}

std::function<QString (const QString&)> dockSetting(QString &name)
{
    return [&name](const QString& s) -> QString {return name + "/" + s;};
}

void MainWindow::writeDockSettings(QDockWidget* dock, QString name)
{
    QSettings settings;
    auto setting = dockSetting(name);
    settings.setValue(setting("dockarea"), dockWidgetArea(dock));
    settings.setValue(setting("floating"), dock->isFloating());
    settings.setValue(setting("visible"), dock->isVisible());
    settings.setValue(setting("size"), dock->size());
    settings.setValue(setting("pos"), dock->pos());
}

void MainWindow::readDockSettings(QDockWidget* dock, QString name)
{
    QSettings settings;
    auto setting = dockSetting(name);
    dock->setVisible(settings.value(setting("visible")).toBool());
    dock->setFloating(settings.value(setting("floating")).toBool());
    setDockSize(dock, settings.value(setting("size"), QSize(300, 300)).toSize());
    dock->move(settings.value(setting("pos"), QPoint(200, 200)).toPoint());
    addDockWidget((Qt::DockWidgetArea)settings.value(setting("dockarea"), Qt::LeftDockWidgetArea).toInt(), dock);
}

void MainWindow::readSettings()
{
    QSettings settings;
    readDockSettings(lyricsDock_, "lyrics");
    readDockSettings(console_, "console");

    restoreGeometry(settings.value("mainwindow/geometry").toByteArray());
    restoreState(settings.value("mainwindow/state").toByteArray());

    cursorFollowsPlayback_ = settings.value("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_).toBool();
    cursorFollowsPlaybackAction->setChecked(cursorFollowsPlayback_);

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
                playlistTabs->setCurrentIndex(lastTab);
        }
    }

    int lastPlayingPosition = settings.value("mainwindow/lastPlayingPosition", -1).toInt();
    if (lastPlayingPosition != -1 && getActivePlaylist()) {
        QModelIndex index = getActivePlaylist()->getUnfilteredPosition(lastPlayingPosition);
        player_.setLastPlayed(getActivePlaylistModel(), index);
        getActivePlaylist()->updateCursorAndScroll(index);
    }
}

void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("mainwindow/maximized", isMaximized());
    settings.setValue("mainwindow/geometry", saveGeometry());
    settings.setValue("mainwindow/state", saveState());
    writeDockSettings(lyricsDock_, "lyrics");
    writeDockSettings(console_, "console");

    settings.setValue("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_);

    settings.setValue("mainwindow/saveTabs", saveTabs_);
    if (saveTabs_) {
        QList<QVariant> names;
        QList<QVariant> data;
        for (int i = 0; i < playlistTabs->count(); ++i) {
            names.append(playlistTabs->tabText(i));
            PlaylistTab* tab = dynamic_cast<PlaylistTab*>(playlistTabs->widget(i));
            if (tab->isEditable()) {
                QByteArray tabData;
                tab->serialize(tabData);
                data.append(tabData);
            } else
                data.append(QVariant::Invalid);
        }
        settings.setValue("mainwindow/tabsNames", QVariant(names));
        settings.setValue("mainwindow/tabsData", QVariant(data));
    }

    int position = -1;
    int lastTab = -1;
    auto lastPlayed = player_.getLastPlayed();
    if (lastPlayed.first && lastPlayed.second.isValid()) {
        auto playlistTab = playlistModels_.left.at(lastPlayed.first);
        position = playlistTab->getUnfilteredPosition(lastPlayed.second);
        lastTab = playlistTabs->indexOf(playlistTab);
    }
    settings.setValue("mainwindow/lastPlayingTab", lastTab);
    settings.setValue("mainwindow/lastPlayingPosition", position);
}

void MainWindow::on_mainToolBar_actionTriggered(QAction* action)
{
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

PlaylistTab* MainWindow::getActivePlaylist()
{
    return dynamic_cast<PlaylistTab *>(playlistTabs->currentWidget());
}

PModel MainWindow::getActivePlaylistModel()
{
    auto playlistTab = dynamic_cast<PlaylistTab *>(playlistTabs->currentWidget());
    if (!playlistTab)
        return nullptr;
    return playlistModels_.right.at(playlistTab);
}

void MainWindow::setTrayIcon(bool playing)
{
    trayIcon_->setIcon(QIcon(playing ? ":/icon/logo22play.png" : ":/icon/logo22.png"));
}

void MainWindow::showHide()
{
    static bool consoleIsVisible = false;
    int currentDesktop = KWindowSystem::currentDesktop();
    WId activeWindow = KWindowSystem::activeWindow();
    if (!isVisible() || isMinimized() || (activeWindow != winId() && activeWindow != console_->winId())) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
        KWindowSystem::setOnDesktop(winId(), currentDesktop);
        if (consoleIsVisible)
            console_->show();
        setVisible(true);
        KWindowSystem::forceActiveWindow(winId());
    } else {
        consoleIsVisible = console_->isVisible();
        console_->hide();
        setVisible(false);
    }
}

void MainWindow::statusBarDoubleClicked()
{
    auto playlistTab = getPlayingPlaylistTab();
    if (playlistTab) {
        playlistTabs->setCurrentWidget(playlistTab);
        playlistTab->updateCursorAndScroll(AudioPlayer::instance->getPlayingIndex());
    }
}

void MainWindow::focusFilter()
{
    if (getActivePlaylist())
        getActivePlaylist()->focusFilter();
}

void MainWindow::updateUI(PTrack track)
{
    int seekMax = 0;
    QString title = "fubar";
    QString tray_tooltip;
    if (track) {
        if (cursorFollowsPlayback_) {
            auto playlistTab = getPlayingPlaylistTab();
            if (playlistTab)
                playlistTab->updateCursor(player_.getPlayingIndex());
        }
        QString track_info = track->metadata["artist"] + " - " + track->metadata["title"];
        title =  track_info + "  [fubar]";
        tray_tooltip = track_info;
        seekMax = track->audioproperties.length;
        lyricsWidget_->setPlainText(track->metadata.value("lyrics"));
    }
    trayIcon_->setToolTip(tray_tooltip);
    setWindowTitle(title);
    seekSlider_->setLimits(0, seekMax);
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        showHide();
    }
}

void MainWindow::volumeChanged(int value)
{
    player_.setVolume(static_cast<qreal>(value) / 100);
}

void MainWindow::enqueueTracks(PModel playlistModel, QModelIndexList tracks)
{
    player_.enqueueTracks(playlistModel, tracks);
}

QModelIndex MainWindow::getCurrentIndex(PModel playlistModel)
{
    auto playlistTab = getPlaylistTab(playlistModel);
    if (!playlistTab)
        return QModelIndex();
    return playlistTab->getCurrentIndex();
}

QModelIndex MainWindow::getFilteredIndex(PModel playlistModel, QModelIndex current, int offset)
{
    auto playlistTab = getPlaylistTab(playlistModel);
    if (!playlistTab)
        return QModelIndex();
    return playlistTab->getFilteredIndex(current, offset);
}

QModelIndex MainWindow::getRandomFilteredIndex(PModel playlistModel)
{
    auto playlistTab = getPlaylistTab(playlistModel);
    if (!playlistTab)
        return QModelIndex();
    return playlistTab->getRandomFilteredIndex();
}

PlaylistTab* MainWindow::getPlaylistTab(PModel playlistModel)
{
    auto it = playlistModels_.left.find(playlistModel);
    return it != playlistModels_.left.end() ? it->second : nullptr;
}

PlaylistTab* MainWindow::getPlayingPlaylistTab()
{
    return getPlaylistTab(player_.getPlayingPlaylistModel());
}

bool MainWindow::isEnqueued(PlaylistTab* playlistTab, QModelIndex index)
{
    // The index is from the sortproxyfiltermodel
    return player_.isEnqueued(playlistModels_.right.at(playlistTab), playlistTab->mapToSource(index));
}

void MainWindow::showHideConsole()
{
    if (console_->isHidden())
        console_->show();
    else
        console_->hide();
}

bool MainWindow::eventFilter(QObject* /*watched*/, QEvent* event)
{
    return event->type() == QEvent::StatusTip;
}

void MainWindow::setDockSize(QDockWidget* dock, QSize size)
{
    QSize oldMaxSize = dock->maximumSize();
    QSize oldMinSize = dock->minimumSize();

    if (size.width() >= 0) {
        if (dock->width() < size.width())
            dock->setMinimumWidth(size.width());
        else dock->setMaximumWidth(size.width());
    }
    if (size.height() >= 0) {
        if (dock->height() < size.height())
            dock->setMinimumHeight(size.height());
        else dock->setMaximumHeight(size.height());
    }
    auto func = [this, dock, oldMinSize, oldMaxSize] () -> void {
        dock->setMinimumSize(oldMinSize);
        dock->setMaximumSize(oldMaxSize);
        QTimer::singleShot(1, this, SLOT(restoreMaximizedState()));
    };
    dockHackFunctions_.push_back(func);
    QTimer::singleShot(1, this, SLOT(returnToOldMaxMinSizes()));
}

void MainWindow::returnToOldMaxMinSizes()
{
    dockHackFunctions_.front()();
    dockHackFunctions_.pop_front();
}

void MainWindow::restoreMaximizedState()
{
    QSettings qsettings;
    if (qsettings.value("mainwindow/maximized", true).toBool())
        KWindowSystem::setState(winId(), NET::Max);
}
