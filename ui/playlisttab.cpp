#include "playlisttab.h"
#include "mainwindow.h"

using boost::shared_ptr;

PlaylistTab::PlaylistTab(QWidget* parent_): QWidget(parent_), model_(playlist_)
{
    ui_.setupUi(this);
    filterModel_.setSourceModel(&model_);
    ui_.playlist->setModel(&filterModel_);

    /*
    try {
        defaultPlaylist->load("test.pb");
    } catch (const std::exception &ex) {
        qDebug(ex.what());
    }
    */

    connect(ui_.filter, SIGNAL(textChanged(QString)), this, SLOT(changedFilter(QString)));
    connect(ui_.playlist, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(play(const QModelIndex &)));
}

void PlaylistTab::changedFilter(const QString &filter)
{
    filterModel_.setFilter(filter);
}

void PlaylistTab::play(const QModelIndex& index)
{
    shared_ptr<Track> track = index.data(TrackRole).value<shared_ptr<Track> >();
    MainWindow::instance->mediaObject->setCurrentSource(Phonon::MediaSource(track->location));
    MainWindow::instance->mediaObject->play();
}

void PlaylistTab::addDirectory(const QString& directory)
{
    model_.addDirectory(directory);
}

#include "playlisttab.moc"