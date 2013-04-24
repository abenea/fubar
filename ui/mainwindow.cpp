#include "mainwindow.h"
#include "library/track.h"
#include "player/audiooutput.h"
#include "player/playlistmodel.h"
#include "playlisttab.h"
#include "playlistfilter.h"
#include "ui/ui_mainwindow.h"
#include "librarypreferencesdialog.h"

#include <Qt>
#include <QTableView>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <QtCore/qmath.h>
#include <kwindowsystem.h>
#include <kaction.h>

MainWindow *MainWindow::instance = 0;

MainWindow::MainWindow(AudioPlayer& player, Library* library, QWidget *parent)
    : QMainWindow(parent)
    , statusBar_(this)
    , player_(player)
    , library_(library)
    , cursorFollowsPlayback_(false)
{
    setupUi(this);

    player.setMainWindow(this);

    QObject::connect(&player_, SIGNAL(randomChanged(bool)), this, SLOT(randomChanged(bool)));
    QObject::connect(&player_, SIGNAL(playingStateChanged(bool)), this, SLOT(playingStateChanged(bool)));
    QObject::connect(&player_, SIGNAL(trackPlaying(PTrack)), this, SLOT(updateUI(PTrack)));
    QObject::connect(&player_, SIGNAL(stopped(qint64, qint64)), this, SLOT(stoppedPlaying()));
    randomChanged(player_.random());

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
    setTrayIcon(true);
    connect(trayIcon_, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon_->show();

    setStatusBar(&statusBar_);
    QObject::connect(&statusBar_, SIGNAL(statusBarDoubleClicked()), this, SLOT(statusBarDoubleClicked()));

    playlistTabs->setTabsClosable(true);
    QObject::connect(playlistTabs, SIGNAL(tabCloseRequested(int)), this, SLOT(removePlaylistTab(int)));

    QObject::connect(menu_File, SIGNAL(aboutToShow()), this, SLOT(menuFileAboutToShow()));

    setWindowIcon(QIcon(":/icon/logo22.png"));

    instance = this;

    on_newLibraryViewAction_triggered();

    readSettings();
    setShortcuts();
}

void MainWindow::addGlobalShortcut(QKeySequence shortcut, QObject* object, const char* slot, QString name)
{
    KAction* action = new KAction(name, this);
    action->setObjectName(name);
    action->setGlobalShortcut(KShortcut(shortcut), KAction::ActiveShortcut, KAction::Autoloading);
    QObject::connect(action, SIGNAL(triggered()), object, slot);

}

void MainWindow::addShortcut(QKeySequence shortcut, const char* func)
{
    QShortcut* q = new QShortcut(shortcut, this);
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

    // App shortcuts
    addShortcut(QKeySequence(Qt::CTRL + Qt::Key_J), SLOT(focusFilter()));
//     addShortcut(QKeySequence(Qt::CTRL + Qt::Key_W), SLOT(removeActivePlaylist()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::randomChanged(bool random)
{
    randomAction->setChecked(random);
}

void MainWindow::playingStateChanged(bool playing)
{
    setTrayIcon(playing);
}

QString msToHumanTime(qint64 pos)
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
        QString progress = msToHumanTime(player_.currentTime()) + " / " + msToHumanTime(track->audioproperties.length * 1000);
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
        tab->addDirectory(directory);
    }
}

void MainWindow::on_addFilesAction_triggered()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select one or more files to open");
    if (files.isEmpty())
        return;

    PlaylistTab *tab = getActivePlaylist();
    if (tab) {
        // Apparently you should iterate over a copy
        QStringList files2 = files;
        tab->addFiles(files2);
    }
}

void MainWindow::addPlaylist(PModel playlistModel, QString name, bool makeCurrent)
{
    PlaylistTab* tab = new PlaylistTab(playlistModel);
    playlistModel->playlist().name = name;
    playlistModels_.insert({playlistModel, tab});
    playlistTabs->addTab(tab, playlistModel->playlist().name);
    if (makeCurrent)
        playlistTabs->setCurrentWidget(tab);
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
    if (library_)
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
    if (library_) {
        LibraryPreferencesDialog* widget = new LibraryPreferencesDialog(*library_, this);
        widget->show();
    }
}

void MainWindow::on_pluginsAction_triggered()
{
    PluginsPreferences* widget = new PluginsPreferences(this);
    widget->show();
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
    player_.setRandom(!player_.random());
}

void MainWindow::readSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("mainwindow/geometry").toByteArray());
    cursorFollowsPlayback_ = settings.value("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_).toBool();
    cursorFollowsPlaybackAction->setChecked(cursorFollowsPlayback_);
}

void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("mainwindow/geometry", saveGeometry());
    settings.setValue("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_);
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
    int currentDesktop = KWindowSystem::currentDesktop();
    if (!isVisible() || isMinimized() || KWindowSystem::activeWindow() != winId()) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
        KWindowSystem::setOnDesktop(winId(), currentDesktop);
        setVisible(true);
        KWindowSystem::forceActiveWindow(winId());
    } else {
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
    if (track) {
        if (cursorFollowsPlayback_) {
            auto playlistTab = getPlayingPlaylistTab();
            if (playlistTab)
                playlistTab->updateCursor(player_.getPlayingIndex());
        }
        title = track->metadata["artist"] + " - " + track->metadata["title"] + "  [fubar]";
        seekMax = track->audioproperties.length;
    }
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

bool MainWindow::isEnqueued(PlaylistTab* playlistTab, PTrack track)
{
    return player_.isEnqueued(playlistModels_.right.at(playlistTab), track);
}

void MainWindow::stoppedPlaying()
{
    updateUI(0);
    statusBar_.clearMessage();
}

#include "mainwindow.moc"