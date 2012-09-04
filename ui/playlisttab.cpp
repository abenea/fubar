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
    connect(ui_.playlist, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(doubleClicked(const QModelIndex &)));
}

void PlaylistTab::changedFilter(const QString &filter)
{
    filterModel_.setFilter(filter);
}

void PlaylistTab::doubleClicked(const QModelIndex& filterIndex)
{
    if (!filterIndex.isValid())
        return;
    return play(filterModel_.mapToSource(filterIndex));
}

void PlaylistTab::play()
{
    QModelIndex filterIndex = ui_.playlist->currentIndex();
    if (filterIndex.isValid()) {
        play(filterModel_.mapToSource(filterIndex));
    } else {
        // Try playing first item in playlist?
        filterIndex = filterModel_.index(0, 0);
        if (filterIndex.isValid()) {
            play(filterModel_.mapToSource(filterIndex));
        }
        else {
            qDebug() << "No clue what item in what playlist play";
        }
    }
}

// Takes a model index, not a filterIndex
void PlaylistTab::play(const QModelIndex& index)
{
    MainWindow::instance->setCurrentPlayingPlaylist(this);
    currentIndex_ = QPersistentModelIndex(index);
    nextIndex_ = QPersistentModelIndex();
    shared_ptr<Track> track = index.data(TrackRole).value<shared_ptr<Track> >();
    MainWindow::instance->mediaObject->setCurrentSource(Phonon::MediaSource(track->location));
    MainWindow::instance->mediaObject->play();
    updateCursor();
}

void PlaylistTab::playNext(int offset)
{
    QModelIndex index = getNextModelIndex(offset);
    if (!index.isValid()) {
        return;
    }
    currentIndex_ = QPersistentModelIndex(index);
    nextIndex_ = QPersistentModelIndex();
    if (!currentIndex_.isValid())
        return;
    play(currentIndex_);
}

void PlaylistTab::enqueueNextTrack()
{
    QModelIndex index = getNextModelIndex(1);
    if (!index.isValid())
        return;
    nextIndex_ = QPersistentModelIndex(index);
    shared_ptr<Track> track = index.data(TrackRole).value<shared_ptr<Track>>();
    MainWindow::instance->mediaObject->enqueue(Phonon::MediaSource(track->location));
    qDebug() << "Enqueue " << track->location;
}

// should this return a persistent index?
QModelIndex PlaylistTab::getNextModelIndex(int offset)
{
    QModelIndex filterIndex = filterModel_.mapFromSource(currentIndex_);
    if (!filterIndex.isValid()) {
//         qDebug() << currentIndex_ << " mapped from source is not good";
        return QModelIndex();
    }
    QModelIndex nextFilterIndex = filterModel_.index(filterIndex.row() + offset, 0);
    if (!nextFilterIndex.isValid()) {
//         qDebug() << filterIndex << " + 1 is not good";
        return QModelIndex();
    }
    return filterModel_.mapToSource(nextFilterIndex);
}

void PlaylistTab::updateCurrentIndex()
{
    if (nextIndex_.isValid()) {
        currentIndex_ = nextIndex_;
        nextIndex_ = QPersistentModelIndex();
        updateCursor();
    }
}

void PlaylistTab::updateCursor()
{
    if (MainWindow::instance->cursorFollowsPlayback()) {
        ui_.playlist->setCurrentIndex(filterModel_.mapFromSource(currentIndex_));
    }
}

void PlaylistTab::updateCursorAndScroll()
{
    ui_.playlist->setCurrentIndex(filterModel_.mapFromSource(currentIndex_));
    ui_.playlist->scrollTo(filterModel_.mapFromSource(currentIndex_), QAbstractItemView::PositionAtCenter);
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

PTrack PlaylistTab::getCurrentTrack()
{
    if (currentIndex_.isValid()) {
        return currentIndex_.data(TrackRole).value<shared_ptr<Track>>();
    }
    return PTrack();
}

#include "playlisttab.moc"
