#include "playlisttab.h"
#include "mainwindow.h"
#include "library/library.h"
#include <memory>

using std::shared_ptr;

PlaylistTab::PlaylistTab(bool synced, QWidget* parent)
    : QWidget(parent)
    , synced_(synced)
    , model_(playlist_)
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
    current_index_ = QPersistentModelIndex(index);
    next_index_ = QPersistentModelIndex();
    shared_ptr<Track> track = index.data(TrackRole).value<shared_ptr<Track> >();
    MainWindow::instance->mediaObject->setCurrentSource(Phonon::MediaSource(track->location));
    MainWindow::instance->mediaObject->play();
    updateCursor();
}

void PlaylistTab::playNext(int offset)
{
    current_index_ = filterModel_.index(current_index_.row() + offset, 0);
    next_index_ = QPersistentModelIndex();
    if (!current_index_.isValid())
        return;
    play(current_index_);
}

void PlaylistTab::enqueueNextTrack()
{
    QModelIndex nextIndex = filterModel_.index(current_index_.row() + 1, 0);
    if (!nextIndex.isValid()) {
        qDebug() << "Nothing to enqueue";
        return;
    }
    next_index_ = QPersistentModelIndex(nextIndex);
    shared_ptr<Track> track = nextIndex.data(TrackRole).value<shared_ptr<Track>>();
    MainWindow::instance->mediaObject->enqueue(Phonon::MediaSource(track->location));
    qDebug() << "Enqueue " << track->location;
}

void PlaylistTab::updateCurrentIndex()
{
    if (next_index_.isValid()) {
        current_index_ = next_index_;
        next_index_ = QPersistentModelIndex();
        updateCursor();
    }
}

void PlaylistTab::updateCursor()
{
    if (MainWindow::instance->cursorFollowsPlayback()) {
        ui_.playlist->setCurrentIndex(current_index_);
    }
}

void PlaylistTab::addDirectory(const QString& directory)
{
    if (!synced_)
        model_.addDirectory(directory);
}

void PlaylistTab::addFiles(const QStringList& files)
{
    if (!synced_)
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
    if (synced_)
        model_.libraryChanged(event);
}

void PlaylistTab::libraryChanged(QList<std::shared_ptr<Track>> tracks)
{
    if (synced_) {
        model_.playlist().tracks.clear();
        model_.playlist().tracks.append(tracks);
    }
}

#include "playlisttab.moc"
