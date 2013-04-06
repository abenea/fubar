#include "mainwindow.h"
#include "../audiooutput.h"
#include "playlistfilter.h"
#include "playlistmodel.h"
#include "ui/ui_mainwindow.h"
#include "ui/librarypreferencesdialog.h"

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

MainWindow::MainWindow(Library* library, AudioOutput* audioOutput, QWidget *parent, bool testing)
    : QMainWindow(parent)
    , statusBar_(this)
    , library_(library)
    , audioOutput_(audioOutput)
    , testing_(testing)
    , currentlyPlayingPlaylist_(nullptr)
    , bufferingTrackPlaylist_(nullptr)
    , cursorFollowsPlayback_(false)
    , random_(false)
{
    setupUi(this);

    // questionable code
    QObject::connect(this, SIGNAL(trackPlaying(PTrack)), this, SLOT(updateUI(PTrack)));
    audioOutput_->setTickInterval(1000);
    QObject::connect(audioOutput_, SIGNAL(aboutToFinish()), this, SLOT(aboutToFinish()));
    QObject::connect(audioOutput_, SIGNAL(currentSourceChanged()), this, SLOT(currentSourceChanged()));
    QObject::connect(audioOutput_, SIGNAL(tick(qint64)), this, SLOT(tick(qint64)));
    // Not using Phono totalTimeChanged() signal because it returns 0 when used with enqueue()
    // TODO: report bug to phonon
//    QObject::connect(audioOutput_, SIGNAL(totalTimeChanged(qint64)), this, SLOT(totalTimeChanged(qint64)));
    seekSlider_ = new SeekSlider(audioOutput_, this);
    // questionable code
    QObject::connect(seekSlider_, SIGNAL(movedByUser(int)), this, SLOT(sliderMovedByUser(int)));
    volumeSlider_ = new QSlider(this);
    volumeSlider_->setOrientation(Qt::Horizontal);
    volumeSlider_->setMaximum(100);
    volumeSlider_->setMinimum(0);
    volumeSlider_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    QObject::connect(volumeSlider_, SIGNAL(valueChanged(int)), this, SLOT(volumeChanged()));
    mainToolBar->addWidget(seekSlider_);
    mainToolBar->addWidget(volumeSlider_);

    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setIcon(QIcon(":/icon/logo.gif"));
    connect(trayIcon_, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon_->show();

    setStatusBar(&statusBar_);
    QObject::connect(&statusBar_, SIGNAL(statusBarDoubleClicked()), this, SLOT(statusBarDoubleClicked()));

    playlistTabs->setTabsClosable(true);
    QObject::connect(playlistTabs, SIGNAL(tabCloseRequested(int)), this, SLOT(removePlaylistTab(int)));
    QObject::connect(menu_File, SIGNAL(aboutToShow()), this, SLOT(menuFileAboutToShow()));

    setWindowIcon(QIcon(":/icon/logo.gif"));

    readSettings();

    instance = this;

    if (testing)
        return;

    on_newLibraryViewAction_triggered();

    setShortcuts();
}

void MainWindow::addShortcut(QKeySequence shortcut, const char* func, QString name)
{
    KAction* action = new KAction(name, this);
    action->setObjectName(name);
    action->setGlobalShortcut(KShortcut(shortcut), KAction::ActiveShortcut, KAction::Autoloading);
    QObject::connect(action, SIGNAL(triggered()), this, func);

}

void MainWindow::setShortcuts()
{
    // Global shortcuts
    addShortcut(QKeySequence(Qt::META + Qt::Key_W), SLOT(showHide()), "Show/Hide");
    addShortcut(QKeySequence(Qt::META + Qt::Key_P), SLOT(showHide()), "Show/Hide");
    addShortcut(QKeySequence(Qt::META + Qt::Key_X), SLOT(play()), "Play");
    addShortcut(QKeySequence(Qt::META + Qt::Key_C), SLOT(playPause()), "Play/Pause");
    addShortcut(QKeySequence(Qt::META + Qt::Key_A), SLOT(prev()), "Prev");
    addShortcut(QKeySequence(Qt::META + Qt::Key_Z), SLOT(next()), "Next");
    addShortcut(QKeySequence(Qt::META + Qt::Key_V), SLOT(stop()), "Stop");

    // App shortcuts
    QShortcut* q = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_J), this);
    QObject::connect(q, SIGNAL(activated()), this, SLOT(focusFilter()));
}

MainWindow::~MainWindow()
{
}

QString msToHumanTime(qint64 pos)
{
    pos /= 1000;
    QString status_message;
    QTextStream(&status_message) << pos / 60 << ":" << qSetPadChar('0') << qSetFieldWidth(2) << pos % 60;
    return status_message;
}

void MainWindow::tick(qint64 pos)
{
    emit trackPositionChanged(pos, false);
    PTrack track = getCurrentTrack();
    if (track) {
        QString progress = msToHumanTime(audioOutput_->currentTime()) + " / " + msToHumanTime(track->audioproperties.length * 1000);
        QString format = QFileInfo(track->location).suffix().toUpper();
        statusBar_.showMessage(QString("%1 %2kbps %3Hz  %4  %5").arg(format).arg(track->audioproperties.bitrate).arg(track->audioproperties.samplerate).arg(QChar(164)).arg(progress));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    stop();
    writeSettings();
    QWidget::closeEvent(event);
}

void MainWindow::addPlaylist(PlaylistTab* playlistTab, QString name)
{
    playlistTabs->addTab(playlistTab, name);
    playlistTabs->setCurrentWidget(playlistTab);
}

void MainWindow::removePlaylistTab(int index)
{
    QWidget* widget = playlistTabs->widget(index);
    if (widget) {
        playlistTabs->removeTab(index);
        delete widget;
    }
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

void MainWindow::on_newLibraryViewAction_triggered()
{
    if (library_) {
        PlaylistTab* tab = new PlaylistTab(true, this);
        tab->addTracks(library_->getTracks());
        QObject::connect(library_, SIGNAL(libraryChanged(LibraryEvent)), tab, SLOT(libraryChanged(LibraryEvent)));
        QObject::connect(library_, SIGNAL(libraryChanged(QList<std::shared_ptr<Track>>)), tab, SLOT(libraryChanged(QList<std::shared_ptr<Track>>)));
        addPlaylist(tab, "All");
    }
}

void MainWindow::on_newPlaylistAction_triggered()
{
    addPlaylist(new PlaylistTab(false, this));
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
//     bool peeked = queue.peeked();
    for (auto index : queue.getTracksAndClear(getActivePlaylist()))
        getActivePlaylist()->repaintTrack(index);
    // We can't buffer next song 'cause gstreamer is bugged to hell
//     if (peeked) {
//         // buffer next song
//         // TODO currentlyPlayingPlaylist_ might be bad
//         currentlyPlayingPlaylist_->enqueueNextTrack();
//     }
}

void MainWindow::on_cursorFollowsPlaybackAction_triggered()
{
    cursorFollowsPlayback_ = !cursorFollowsPlayback_;
}

void MainWindow::on_randomAction_triggered()
{
    random_ = !random_;
}

void MainWindow::readSettings()
{
    if (testing_)
        return;
    QSettings settings;
    restoreGeometry(settings.value("mainwindow/geometry").toByteArray());
    qreal volume = settings.value("mainwindow/volume", -1).toReal();
    if (volume >= 0) {
        volumeSlider_->setValue(volume * 100);
        setVolume(volume);
    }
    cursorFollowsPlayback_ = settings.value("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_).toBool();
    cursorFollowsPlaybackAction->setChecked(cursorFollowsPlayback_);
    random_ = settings.value("mainwindow/random", random_).toBool();
    randomAction->setChecked(random_);
}

void MainWindow::writeSettings()
{
    if (testing_)
        return;
    QSettings settings;
    settings.setValue("mainwindow/geometry", saveGeometry());
    settings.setValue("mainwindow/volume", currentVolume());
    settings.setValue("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_);
    settings.setValue("mainwindow/random", random_);
}

void MainWindow::setCurrentPlayingPlaylist(PlaylistTab* playlist)
{
    currentlyPlayingPlaylist_ = playlist;
}

void MainWindow::on_mainToolBar_actionTriggered(QAction* action)
{
    if (action->text().toLower().contains("play")) {
        play();
    } else if (action->text().toLower().contains("pause")) {
        playPause();
    } else if (action->text().toLower().contains("stop")) {
        stop();
    } else if (action->text().toLower().contains("prev")) {
        prev();
    } else if (action->text().toLower().contains("next")) {
        next();
    }
}

void MainWindow::enqueueTrack(PTrack track)
{
    audioOutput_->clearQueue();
    audioOutput_->enqueue(track->location);
    bufferingTrack_ = track;
    qDebug() << "Enqueue " << track->location;
}

void MainWindow::playTrack(PTrack track)
{
    bufferingTrack_ = nullptr;
    audioOutput_->clearQueue();
    audioOutput_->setCurrentSource(track->location);
    volumeChanged();
    audioOutput_->play();
}

void MainWindow::aboutToFinish()
{
    if (!queue.isEmpty()) {
        auto enqueued = queue.peekTrack();
        if (enqueued.second.isValid()) {
            if (currentlyPlayingPlaylist_ != enqueued.first) {
                bufferingTrackPlaylist_ = enqueued.first;
            }
            enqueued.first->enqueueTrack(enqueued.second);
            return;
        }
    }
    currentlyPlayingPlaylist_->enqueueNextTrack();
}

void MainWindow::currentSourceChanged()
{
    qDebug() << "currentSourceChanged() " << audioOutput_->currentTime() << " " << audioOutput_->totalTime();
    if (bufferingTrackPlaylist_) {
        if (playlistTabs->indexOf(bufferingTrackPlaylist_) != -1)
            currentlyPlayingPlaylist_ = bufferingTrackPlaylist_;
        else
            currentlyPlayingPlaylist_ = nullptr;
        bufferingTrackPlaylist_= nullptr;
    }
    if (queue.peeked()) {
        auto enqueued =  queue.popPeekedTrack();
        repaintEnqueuedTrack(enqueued.second);
    }
    PTrack track = nullptr;
    if (currentlyPlayingPlaylist_) {
        currentlyPlayingPlaylist_->currentSourceChanged();
        track = currentlyPlayingPlaylist_->getCurrentTrack();
    }
    if (bufferingTrack_)
        track = bufferingTrack_;
    if (track) {
        volumeChanged();
        emit trackPlaying(track);
        emit trackPositionChanged(0, true);
    } else {
        qDebug() << "Source changed to null track!";
    }
    bufferingTrack_ = nullptr;
}

PlaylistTab* MainWindow::getActivePlaylist()
{
    return dynamic_cast<PlaylistTab *>(playlistTabs->currentWidget());
}


PTrack MainWindow::getCurrentTrack()
{
    PlaylistTab* playlist = currentlyPlayingPlaylist_;
    if (!playlist)
        return PTrack(0);
    return playlist->getCurrentTrack();
}

void MainWindow::play()
{
    auto playlistTab = currentlyPlayingPlaylist_;
    if (!playlistTab)
        playlistTab = getActivePlaylist();
    if (playlistTab) {
        playlistTab->play();
        PTrack track = getCurrentTrack();
        if (track) {
            emit trackPlaying(track);
            emit trackPositionChanged(0, true);
        }
    }
}

void MainWindow::playPause()
{
    if (audioOutput_->paused())
        audioOutput_->play();
    else
        audioOutput_->pause();
}

void MainWindow::next()
{
    if (!queue.isEmpty()) {
        auto enqueued = queue.popTrack();
        if (enqueued.second.isValid()) {
            currentlyPlayingPlaylist_ = enqueued.first;
            currentlyPlayingPlaylist_->play(enqueued.second);
            repaintEnqueuedTrack(enqueued.second);
            return;
        }
    }
    if (currentlyPlayingPlaylist_)
        currentlyPlayingPlaylist_->playNext(+1);
}

void MainWindow::prev()
{
    if (currentlyPlayingPlaylist_)
        currentlyPlayingPlaylist_->playNext(-1);
}

void MainWindow::stop()
{
    audioOutput_->stop();
    PTrack track = getCurrentTrack();
    if (track)
        emit stopped(audioOutput_->totalTime(), audioOutput_->currentTime());
    updateUI(0);
    statusBar_.clearMessage();
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
    if (currentlyPlayingPlaylist_) {
        playlistTabs->setCurrentWidget(currentlyPlayingPlaylist_);
        currentlyPlayingPlaylist_->updateCursorAndScroll();
    }
}

void MainWindow::focusFilter()
{
    if (getActivePlaylist())
        getActivePlaylist()->focusFilter();
}

void MainWindow::updateUI(PTrack track)
{
    if (track) {
        setWindowTitle(track->metadata["artist"] + " - " + track->metadata["title"] + "  [fubar]");
        seekSlider_->setLimits(0, track->audioproperties.length);
    } else {
        setWindowTitle("fubar");
        seekSlider_->setLimits(0, 0);
    }
}

void MainWindow::totalTimeChanged(qint64 time)
{
    qDebug() << "Total time changed " << time;
}

void MainWindow::sliderMovedByUser(int pos)
{
    emit trackPositionChanged(pos, true);
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        showHide();
    }
}

void MainWindow::repaintEnqueuedTrack(const QPersistentModelIndex& index)
{
    if (!currentlyPlayingPlaylist_ || currentlyPlayingPlaylist_ != getActivePlaylist())
        return;
    currentlyPlayingPlaylist_->repaintTrack(index);
}

void MainWindow::volumeChanged()
{
    setVolume(currentVolume());
}

qreal MainWindow::currentVolume()
{
    if (volumeSlider_->value() == 0)
        return 0;
    return static_cast<qreal>(volumeSlider_->value()) / 100;
}

void MainWindow::setVolume(qreal value)
{
    audioOutput_->setVolume(value);
    return;

    // TODO fix album formula and write a config UI
    PTrack track = getCurrentTrack();
    qreal volume = value;
    if (track) {
        // gain = 10 ^ ((rg + pream) / 20)
        QMap<QString, QString>::const_iterator it = track->metadata.find("REPLAYGAIN_TRACK_GAIN");
        qreal rg;
        if (it != track->metadata.end()) {
            QString track_rg = it.value();
            track_rg.chop(3);
            rg = track_rg.toDouble();
            qreal preamp = 10;
            rg = qPow(10, (preamp + rg) / 20);
            qDebug() << "Got track replaygain " << track_rg << " resulting in gain = " << rg;
        } else {
            rg = 1;
        }
        volume *= rg;
    }
//     qDebug() << "Setting volume to " << volume;
    audioOutput_->setVolume(volume);
}

#include "mainwindow.moc"