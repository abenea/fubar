#include "mainwindow.h"
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
#include <kwindowsystem.h>
#include <kaction.h>

using namespace boost;

MainWindow *MainWindow::instance = 0;

MainWindow::MainWindow(Library& library, QWidget *parent)
    : QMainWindow(parent)
    , statusBar_(this)
    , library_(library)
    , currentlyPlayingPlaylist_(0)
    , cursorFollowsPlayback_(false)
    , random_(false)
{
    setupUi(this);

    // questionable code
    QObject::connect(this, SIGNAL(trackPlaying(PTrack)), this, SLOT(updateUI(PTrack)));
    audioOutput = new Phonon::AudioOutput(Phonon::MusicCategory, this);
    mediaObject = new Phonon::MediaObject(this);
    mediaObject->setTickInterval(1000);
    Phonon::createPath(mediaObject, audioOutput);
    QObject::connect(mediaObject, SIGNAL(aboutToFinish()), this, SLOT(aboutToFinish()));
    QObject::connect(mediaObject, SIGNAL(currentSourceChanged(const Phonon::MediaSource &)), this, SLOT(currentSourceChanged(const Phonon::MediaSource &)));
    QObject::connect(mediaObject, SIGNAL(tick(qint64)), this, SLOT(tick(qint64)));
    // TODO: report bug to phonon about totalTimeChanged reporting crap when enqueue is used
//    QObject::connect(mediaObject, SIGNAL(totalTimeChanged(qint64)), this, SLOT(totalTimeChanged(qint64)));
    seekSlider_ = new SeekSlider(mediaObject, this);
    // questionable code
    QObject::connect(seekSlider_, SIGNAL(movedByUser(int)), this, SLOT(sliderMovedByUser(int)));
    volumeSlider_ = new Phonon::VolumeSlider(this);
    volumeSlider_->setAudioOutput(audioOutput);
    volumeSlider_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    mainToolBar->addWidget(seekSlider_);
    mainToolBar->addWidget(volumeSlider_);

    setStatusBar(&statusBar_);
    QObject::connect(&statusBar_, SIGNAL(statusBarDoubleClicked()), this, SLOT(statusBarDoubleClicked()));

    setWindowIcon(QIcon(":/icon/logo.gif"));

    readSettings();

    instance = this;

    on_newLibraryViewAction_triggered();

    setShortcuts();
}

void MainWindow::addShortcut(QKeySequence shortcut, const char* func, QString name)
{
    // Not saving a reference to it
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
        QString progress = msToHumanTime(mediaObject->currentTime()) + " / " + msToHumanTime(track->audioproperties.length * 1000);
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

PlaylistTab* MainWindow::current()
{
    return dynamic_cast<PlaylistTab *>(playlistTabs->currentWidget());
}

void MainWindow::on_addDirectoryAction_triggered()
{
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString directory = QFileDialog::getExistingDirectory(this, tr("Add directory"), QString(),
                                                          options);
    if (directory.isEmpty())
        return;

    PlaylistTab *tab = current();
    if (tab) {
        tab->addDirectory(directory);
    }
}

void MainWindow::on_addFilesAction_triggered()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select one or more files to open");
    if (files.isEmpty())
        return;

    PlaylistTab *tab = current();
    if (tab) {
        // Apparently you should iterate over a copy
        QStringList files2 = files;
        tab->addFiles(files2);
    }
}

void MainWindow::on_newLibraryViewAction_triggered()
{
    PlaylistTab* tab = new PlaylistTab(true, this);
    tab->addTracks(library_.getTracks());
    QObject::connect(&library_, SIGNAL(libraryChanged(LibraryEvent)), tab, SLOT(libraryChanged(LibraryEvent)));
    QObject::connect(&library_, SIGNAL(libraryChanged(QList<std::shared_ptr<Track>>)), tab, SLOT(libraryChanged(QList<std::shared_ptr<Track>>)));
    playlistTabs->addTab(tab, "All");
}

void MainWindow::on_newPlaylistAction_triggered()
{
    playlistTabs->addTab(new PlaylistTab(false, this), "Unnamed playlist");
}

void MainWindow::on_preferencesAction_triggered()
{
    LibraryPreferencesDialog* widget = new LibraryPreferencesDialog(library_, this);
    widget->show();
}

void MainWindow::on_cursorFollowsPlaybackAction_triggered()
{
    cursorFollowsPlayback_ = !cursorFollowsPlayback_;
}

void MainWindow::on_random_triggered()
{
    random_ = !random_;
}

void MainWindow::readSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("mainwindow/geometry").toByteArray());
    audioOutput->setVolume(settings.value("mainwindow/volume", audioOutput->volume()).toReal());
    cursorFollowsPlayback_ = settings.value("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_).toBool();
    cursorFollowsPlaybackAction->setChecked(cursorFollowsPlayback_);
}

void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("mainwindow/geometry", saveGeometry());
    settings.setValue("mainwindow/volume", audioOutput->volume());
    settings.setValue("mainwindow/cursorFollowsPlayback", cursorFollowsPlayback_);
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

void MainWindow::aboutToFinish()
{
    currentlyPlayingPlaylist_->enqueueNextTrack();
}

void MainWindow::currentSourceChanged(const Phonon::MediaSource& /*source*/)
{
    qDebug() << "currentSourceChanged() " << mediaObject->currentTime() << " " << mediaObject->totalTime();
    currentlyPlayingPlaylist_->updateCurrentIndex();
    if (currentlyPlayingPlaylist_->getCurrentTrack()) {
        emit trackPlaying(currentlyPlayingPlaylist_->getCurrentTrack());
        emit trackPositionChanged(0, true);
    } else {
        qDebug() << "OMFG! source changed to null track!!!";
    }
}

PlaylistTab* MainWindow::getCurrentPlaylist()
{
    if (currentlyPlayingPlaylist_ == 0) {
        setCurrentPlayingPlaylist(current());
    }
    return currentlyPlayingPlaylist_;
}

PTrack MainWindow::getCurrentTrack()
{
    PlaylistTab* playlist = getCurrentPlaylist();
    if (!playlist)
        return PTrack(0);
    return playlist->getCurrentTrack();
}

void MainWindow::play()
{
    if (getCurrentPlaylist()) {
        currentlyPlayingPlaylist_->play();
        PTrack track = getCurrentTrack();
        if (track) {
            emit trackPlaying(track);
            emit trackPositionChanged(0, true);
        }
    }
}

void MainWindow::playPause()
{
    if (mediaObject->state() == Phonon::PausedState)
        mediaObject->play();
    else
        mediaObject->pause();
}

void MainWindow::next()
{
    if (getCurrentPlaylist())
        currentlyPlayingPlaylist_->playNext(+1);
}

void MainWindow::prev()
{
    if (getCurrentPlaylist())
        currentlyPlayingPlaylist_->playNext(-1);
}

void MainWindow::stop()
{
    mediaObject->stop();
    PTrack track = getCurrentTrack();
    if (track)
        emit stopped(mediaObject->totalTime(), mediaObject->currentTime());
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
    PlaylistTab* playlist = getCurrentPlaylist();
    if (playlist) {
        playlist->focusFilter();
    }
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

#include "mainwindow.moc"