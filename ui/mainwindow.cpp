#include "mainwindow.h"
#include "playlistfilter.h"
#include "playlistmodel.h"
#include "ui/ui_mainwindow.h"
#include "ui/librarypreferencesdialog.h"

#include <QTableView>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <kwindowsystem.h>
#include <QxtGlobalShortcut>
#include <Qt>

using namespace boost;

MainWindow *MainWindow::instance = 0;

MainWindow::MainWindow(Library& library, QWidget *parent)
    : QMainWindow(parent)
    , library_(library)
    , currentlyPlayingPlaylist_(0)
    , cursorFollowsPlayback_(false)
{
    setupUi(this);

    audioOutput = new Phonon::AudioOutput(Phonon::MusicCategory, this);
    mediaObject = new Phonon::MediaObject(this);
    mediaObject->setTickInterval(1000);
    Phonon::createPath(mediaObject, audioOutput);
    QObject::connect(mediaObject, SIGNAL(aboutToFinish()), this, SLOT(AboutToFinish()));
    seekSlider_ = new Phonon::SeekSlider(this);
    seekSlider_->setIconVisible(false);
    seekSlider_->setMediaObject(mediaObject);
    volumeSlider_ = new Phonon::VolumeSlider(this);
    volumeSlider_->setAudioOutput(audioOutput);
    volumeSlider_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    mainToolBar->addWidget(seekSlider_);
    mainToolBar->addWidget(volumeSlider_);

//     playlistTabs->addTab(new PlaylistTab(this), "~/music_test");
//     current()->addDirectory("/home/bogdan/music_test");

    readSettings();

    instance = this;

    on_newLibraryViewAction_triggered();

    SetShortcuts();
}

void MainWindow::AddShortcut(QKeySequence shortcut, const char* func)
{
    // Not saving a reference to it
    QxtGlobalShortcut* gs = new QxtGlobalShortcut(this);
    QObject::connect(gs, SIGNAL(activated()), this, func);
    gs->setShortcut(shortcut);
}

void MainWindow::SetShortcuts()
{
    AddShortcut(QKeySequence(Qt::META + Qt::Key_W), SLOT(ShowHide()));
    AddShortcut(QKeySequence(Qt::META + Qt::Key_X), SLOT(Play()));
    AddShortcut(QKeySequence(Qt::META + Qt::Key_C), SLOT(PlayPause()));
    AddShortcut(QKeySequence(Qt::META + Qt::Key_A), SLOT(Prev()));
    AddShortcut(QKeySequence(Qt::META + Qt::Key_Z), SLOT(Next()));
    AddShortcut(QKeySequence(Qt::META + Qt::Key_V), SLOT(Stop()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::tick(qint64 )
{
    
}

void MainWindow::closeEvent(QCloseEvent* event)
{
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
    PlaylistTab* tab = new PlaylistTab(this);
    tab->addTracks(library_.getTracks());
    QObject::connect(&library_, SIGNAL(libraryChanged(LibraryEvent)), tab, SLOT(libraryChanged(LibraryEvent)));
    tab->yunorefresh();
    playlistTabs->addTab(tab, "All");
}

void MainWindow::on_newPlaylistAction_triggered()
{
    playlistTabs->addTab(new PlaylistTab(this), "Unnamed playlist");
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

void MainWindow::readSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("mainwindow/geometry").toByteArray());
}

void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("mainwindow/geometry", saveGeometry());
}

void MainWindow::setCurrentPlayingPlaylist(PlaylistTab* playlist)
{
    currentlyPlayingPlaylist_ = playlist;
}

void MainWindow::on_mainToolBar_actionTriggered(QAction* action)
{
    if (action->text().toLower().contains("play")) {
        Play();
    } else if (action->text().toLower().contains("pause")) {
        PlayPause();
    } else if (action->text().toLower().contains("stop")) {
        Stop();
    } else if (action->text().toLower().contains("prev")) {
        Prev();
    } else if (action->text().toLower().contains("next")) {
        Next();
    }
}

void MainWindow::AboutToFinish()
{
    currentlyPlayingPlaylist_->enqueueNextTrack();
}

PlaylistTab* MainWindow::getCurrentPlaylist()
{
    if (currentlyPlayingPlaylist_ == 0) {
        setCurrentPlayingPlaylist(current());
    }
    return currentlyPlayingPlaylist_;
}

void MainWindow::Play()
{
    if (getCurrentPlaylist())
        currentlyPlayingPlaylist_->play();
}

void MainWindow::PlayPause()
{
    if (mediaObject->state() == Phonon::PausedState)
        mediaObject->play();
    else
        mediaObject->pause();
}

void MainWindow::Next()
{
    if (getCurrentPlaylist())
        currentlyPlayingPlaylist_->playNext(mediaObject->currentSource().fileName(), +1);
}

void MainWindow::Prev()
{
    if (getCurrentPlaylist())
        currentlyPlayingPlaylist_->playNext(mediaObject->currentSource().fileName(), -1);
}

void MainWindow::Stop()
{
    mediaObject->stop();
}

void MainWindow::ShowHide()
{
    if(!isVisible())
    {
        setVisible(true);
    }
    else
    {
        int currentDesktop = KWindowSystem::currentDesktop();
        if(!isMinimized())
        {
            if(!isActiveWindow()) // not minimised and without focus
            {
                KWindowSystem::setOnDesktop(winId(), currentDesktop);
                activateWindow();
            }
            else // has focus
            {
                setVisible(false);
            }
        }
        else // is minimised
        {
            setWindowState(windowState() & ~Qt::WindowMinimized);
            KWindowSystem::setOnDesktop(winId(), currentDesktop);
            activateWindow();
        }
    }
}

#include "mainwindow.moc"