#include "mainwindow.h"
#include "playlistfilter.h"
#include "playlistmodel.h"
#include "ui/ui_mainwindow.h"
#include "ui/librarypreferencesdialog.h"

#include <QTableView>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>

using namespace boost;

MainWindow *MainWindow::instance = 0;

MainWindow::MainWindow(Library& library, QWidget *parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindowClass)
    , library_(library)
{
    ui_->setupUi(this);

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
    ui_->mainToolBar->addWidget(seekSlider_);
    ui_->mainToolBar->addWidget(volumeSlider_);

//     ui_->playlistTabs->addTab(new PlaylistTab(this), "~/music_test");
//     current()->addDirectory("/home/bogdan/music_test");

    connect(ui_->actionAdd_directory, SIGNAL(triggered(bool)), this, SLOT(addDirectory()));
    connect(ui_->actionPreferences, SIGNAL(triggered(bool)), this, SLOT(libraryPreferences()));

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
    return dynamic_cast<PlaylistTab *>(ui_->playlistTabs->currentWidget());
}

void MainWindow::addDirectory()
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

void MainWindow::libraryPreferences()
{
    LibraryPreferencesDialog* widget = new LibraryPreferencesDialog(library_, this);
    widget->show();
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

void MainWindow::addView(PlaylistTab* playlistTab, const QString& name)
{
    ui_->playlistTabs->addTab(playlistTab, name);
}

void MainWindow::on_mainToolBar_actionTriggered(QAction* action)
{
    if (action->text().toLower().contains("play")) {
        current()->play();
    } else if (action->text().toLower().contains("pause")) {
        if (mediaObject->state() == Phonon::PausedState)
            mediaObject->play();
        else
            mediaObject->pause();
    } else if (action->text().toLower().contains("stop")) {
        mediaObject->stop();
    } else if (action->text().toLower().contains("prev")) {
    } else if (action->text().toLower().contains("next")) {
        current()->playNext(mediaObject->currentSource().fileName());
    }
}

#include "mainwindow.moc"