#include "playlisttab.h"
#include "mainwindow.h"
#include "playlistmodel.h"
#include "library/library.h"
#include <QDebug>
#include <QUrl>
#include <memory>

using std::shared_ptr;

PlaylistTab::PlaylistTab(PModel model, QWidget* parent)
    : QWidget(parent)
    , model_(model)
{
    ui_.setupUi(this);
    filterModel_.setSourceModel(model_.get());
    filterModel_.setDynamicSortFilter(model_->playlist().synced);
    if (model_->playlist().synced)
        filterModel_.sort(0);
    ui_.playlist->setModel(&filterModel_);

    connect(model_.get(), SIGNAL(queueStatusChanged(std::vector<QPersistentModelIndex>)), this, SLOT(repaintTracks(std::vector<QPersistentModelIndex>)));
    connect(ui_.filter, SIGNAL(textChanged(QString)), this, SLOT(changedFilter(QString)));
    connect(ui_.filter, SIGNAL(returnPressed()), this, SLOT(clearFilterAndPlay()));
    connect(ui_.playlist, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(doubleClicked(const QModelIndex &)));
    connect(ui_.playlist, SIGNAL(returnPressed(QModelIndex)), this, SLOT(doubleClicked(const QModelIndex &)));

    ui_.playlist->setDragEnabled(true);
    ui_.playlist->setDropIndicatorShown(isEditable());
    ui_.playlist->setAcceptDrops(isEditable());
}

bool PlaylistTab::isEditable() const
{
    return !model_->playlist().synced;
}

void PlaylistTab::changedFilter(const QString &filter)
{
    QPersistentModelIndex modelIndex;
    if (filter.isEmpty()) {
        QModelIndex filterIndex = ui_.playlist->currentIndex();
        modelIndex = QPersistentModelIndex(filterModel_.mapToSource(filterIndex));
    }
    filterModel_.setFilter(filter);

    if (!filter.isEmpty()) {
        QModelIndex filterIndex = filterModel_.index(0, 0);
        if (filterIndex.isValid()) {
            ui_.playlist->setCurrentIndex(filterIndex);
        }
    } else {
        QModelIndex filterIndex = filterModel_.mapFromSource(modelIndex);
        if (filterIndex.isValid()) {
            ui_.playlist->scrollTo(filterIndex, QAbstractItemView::PositionAtCenter);
            ui_.playlist->setCurrentIndex(filterIndex);
            // Lame code: Second time's the charm
            // No clue why first time does not paint the track as selected
            ui_.playlist->setCurrentIndex(filterIndex);
        }
    }
}

void PlaylistTab::doubleClicked(const QModelIndex& filterIndex)
{
    if (!filterIndex.isValid())
        return;
    AudioPlayer::instance->play(model_, filterModel_.mapToSource(filterIndex));
}

void PlaylistTab::clearFilterAndPlay()
{
    auto index = filterModel_.mapToSource(filterModel_.index(0, 0));
    ui_.filter->clear();
    ui_.playlist->setFocus();
    if (index.isValid()) {
        AudioPlayer::instance->play(model_, index);
        ui_.playlist->scrollTo(filterModel_.mapFromSource(index), QAbstractItemView::PositionAtTop);
    }
}

void PlaylistTab::focusFilter()
{
    ui_.filter->setFocus();
}

void PlaylistTab::updateCursor(QModelIndex index)
{
    ui_.playlist->setCurrentIndex(filterModel_.mapFromSource(index));
}

void PlaylistTab::updateCursorAndScroll(QModelIndex index)
{
    updateCursor(index);
    ui_.playlist->scrollTo(filterModel_.mapFromSource(index), QAbstractItemView::PositionAtCenter);
}

void PlaylistTab::repaintTracks(std::vector<QPersistentModelIndex> indexes)
{
    for (const auto& index : indexes)
        ui_.playlist->update(filterModel_.mapFromSource(index));
}

void PlaylistTab::addUrls(const QList<QUrl>& urls) {
    if (!model_->playlist().synced)
        model_->addUrls(urls);
}

void PlaylistTab::addTracks(const QList<shared_ptr<Track>>& tracks)
{
    model_->addTracks(tracks);
}

void PlaylistTab::removeTracks(QModelIndexList trackList)
{
    if (!model_->playlist().synced)
        model_->removeIndexes(mapToSource(trackList));
}

QModelIndexList PlaylistTab::mapToSource(QModelIndexList indexes) const
{
    QModelIndexList result;
    for (auto index : indexes)
        result.append(filterModel_.mapToSource(index));
    return result;
}

QModelIndex PlaylistTab::mapToSource(QModelIndex index) const
{
    return filterModel_.mapToSource(index);
}

QModelIndex PlaylistTab::getCurrentIndex()
{
    return filterModel_.mapToSource(ui_.playlist->currentIndex());
}

QModelIndex PlaylistTab::getFilteredIndex(QModelIndex current, int offset)
{
    QModelIndex filterIndex = filterModel_.mapFromSource(current);
    // If current index is filtered out, play first track from filtered list
    if (!filterIndex.isValid()) {
        filterIndex = filterModel_.index(0, 0);
        if (!filterIndex.isValid()) {
            return QModelIndex();
        }
    } else {
        filterIndex = filterModel_.index(filterIndex.row() + offset, 0);
        if (!filterIndex.isValid()) {
            return QModelIndex();
        }
    }
    return filterModel_.mapToSource(filterIndex);
}

QModelIndex PlaylistTab::getRandomFilteredIndex()
{
    if (filterModel_.rowCount() == 0)
        return QModelIndex();
    QModelIndex filterIndex = filterModel_.index(rand() % filterModel_.rowCount(), 0);
    return filterModel_.mapToSource(filterIndex);
}

int PlaylistTab::getUnfilteredPosition(QModelIndex index)
{
    // this actually clears the filter but we don't care
    // because it's only called at exit
    filterModel_.setFilter("");
    return filterModel_.mapFromSource(index).row();
}

QModelIndex PlaylistTab::getUnfilteredPosition(int pos)
{
    return filterModel_.mapToSource(filterModel_.index(pos, 0));
}

void PlaylistTab::serialize(QByteArray& data) const
{
    model_->playlist().serialize(data);
}

void PlaylistTab::deserialize(const QByteArray& data)
{
    model_->deserialize(data);
}

bool PlaylistTab::dropMimeData(const QMimeData* data)
{
    model_->dropMimeData(data, Qt::CopyAction, 0, 0, QModelIndex());
    return false;
}
