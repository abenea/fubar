#include "playlisttab.h"
#include "mainwindow.h"
#include "library/library.h"
#include <memory>

using std::shared_ptr;

PlaylistTab::PlaylistTab(QWidget* parent): QWidget(parent), model_(playlist_)
{
    ui_.setupUi(this);
    filterModel_.setSourceModel(&model_);
    filterModel_.setDynamicSortFilter(true);
    filterModel_.sort(0);
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

void PlaylistTab::play()
{
    QModelIndex index = ui_.playlist->currentIndex();
    if (index.isValid())
        play(index);
    else {
        // Try playing first item in playlist?
        index = filterModel_.index(0, 0);
        if (index.isValid()) {
            play(index);
        }
        else {
            qDebug() << "No clue what item in what playlist play";
        }
    }
}

// index from QListView - mapped from model to proxy
// maybe it should be model index?
void PlaylistTab::play(const QModelIndex& index)
{
    MainWindow::instance->setCurrentPlayingPlaylist(this);
    shared_ptr<Track> track = index.data(TrackRole).value<shared_ptr<Track> >();
    MainWindow::instance->mediaObject->setCurrentSource(Phonon::MediaSource(track->location));
    MainWindow::instance->mediaObject->play();
    if (MainWindow::instance->cursorFollowsPlayback()) {
        ui_.playlist->setCurrentIndex(index);
    }
}

void PlaylistTab::playNext(QString path, int offset)
{
    QModelIndex index = model_.getIndex(path);
    if (!index.isValid())
        return;
    QModelIndex indexMapped = filterModel_.mapFromSource(index);
    if (!indexMapped.isValid())
        return;
    QModelIndex nextIndex = filterModel_.index(indexMapped.row() + offset, 0);
    if (!nextIndex.isValid())
        return;
    play(nextIndex);
}

void PlaylistTab::addDirectory(const QString& directory)
{
    model_.addDirectory(directory);
}

void PlaylistTab::addFiles(const QStringList& files)
{
    model_.addFiles(files);
}

void PlaylistTab::yunorefresh()
{
    model_.yunorefresh();
}

void PlaylistTab::addTracks(const QList< shared_ptr< Track > >& tracks)
{
	model_.playlist().tracks.append(tracks);
}

void PlaylistTab::libraryChanged(LibraryEvent event)
{
	model_.libraryChanged(event);
}

#include "playlisttab.moc"
