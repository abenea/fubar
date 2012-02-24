#include "mainwindow.h"
#include "library/viewmanager.h"
#include "playlistfilter.h"
#include "playlistmodel.h"
#include "ui/ui_mainwindow.h"
#include "ui/librarypreferencesdialog.h"
#include "libraryviewplaylist.h"

#include <QTableView>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>

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
}

MainWindow::~MainWindow()
{
}

void MainWindow::tick(qint64 pos)
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
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
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
    addView(ViewManager::instance->createView(), "All");
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

void MainWindow::addView(LibraryViewPlaylist* view, const QString& name)
{
    playlistTabs->addTab(view, name);
}

void MainWindow::setCurrentPlayingPlaylist(PlaylistTab* playlist)
{
    currentlyPlayingPlaylist_ = playlist;
}

void MainWindow::on_mainToolBar_actionTriggered(QAction* action)
{
    if (action->text().toLower().contains("play")) {
        if (currentlyPlayingPlaylist_ == 0)
            currentlyPlayingPlaylist_ = current();
        currentlyPlayingPlaylist_->play();
    } else if (action->text().toLower().contains("pause")) {
        if (mediaObject->state() == Phonon::PausedState)
            mediaObject->play();
        else
            mediaObject->pause();
    } else if (action->text().toLower().contains("stop")) {
        mediaObject->stop();
    } else if (action->text().toLower().contains("prev")) {
        currentlyPlayingPlaylist_->playNext(mediaObject->currentSource().fileName(), -1);
    } else if (action->text().toLower().contains("next")) {
        currentlyPlayingPlaylist_->playNext(mediaObject->currentSource().fileName(), +1);
    }
}

#include "mainwindow.moc"