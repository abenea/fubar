#include "playlistfilter.h"
#include "playlistmodel.h"
#include "library/track.h"

#include <boost/cast.hpp>
using namespace std;

PlaylistFilter::PlaylistFilter(QObject *parent) :
        QSortFilterProxyModel(parent)
{
    connect(this, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
            this, SLOT(invalidateGrouping()));
    connect(this, SIGNAL(layoutChanged()),
            this, SLOT(invalidateGrouping()));
    connect(this, SIGNAL(modelReset()),
            this, SLOT(invalidateGrouping()));
    connect(this, SIGNAL(rowsInserted(const QModelIndex&, int, int)),
            this, SLOT(invalidateGrouping()));
    connect(this, SIGNAL(rowsRemoved(const QModelIndex&, int, int)),
            this, SLOT(invalidateGrouping()));
}

void PlaylistFilter::setFilter(const QString& filter)
{
    filter_ = filter.split(" ", QString::SkipEmptyParts);
    invalidateFilter();
}

bool PlaylistFilter::filterAcceptsRow(int source_row, const QModelIndex &/*source_parent*/) const
{
    PlaylistModel *pm = boost::polymorphic_cast<PlaylistModel *>(sourceModel());
    shared_ptr<Track> track = pm->playlist().tracks[source_row];

    foreach (const QString &keyword, filter_) {
        if (track->location.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }
        bool found = false;
        foreach (const QString &value, track->metadata) {
            if (value.contains(keyword, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool PlaylistFilter::sameGroup(shared_ptr<Track> track1, shared_ptr<Track> track2)
{
    if (track1 && track2) {
        return (track1->metadata["artist"] == track2->metadata["artist"] ||
            (track1->metadata.value("album artist", "") == track2->metadata.value("album artist", "") &&
                !track1->metadata.value("album artist", "").isEmpty())) &&
            track1->metadata["album"] == track2->metadata["album"] &&
            track1->metadata["year"] == track2->metadata["year"];
    } else {
        return false;
    }
}

Grouping::Mode PlaylistFilter::groupingMode(const QModelIndex& index) const
{
    Grouping::Mode mode = groupingMode_.value(index.row(), Grouping::Invalid);
    if (mode != Grouping::Invalid)
        return mode;

    QModelIndex prevIndex(index.sibling(index.row() - 1, index.column()));
    QModelIndex nextIndex(index.sibling(index.row() + 1, index.column()));

    PTrack prevTrack = prevIndex.data(TrackRole).value<shared_ptr<Track>>();
    PTrack currentTrack = index.data(TrackRole).value<shared_ptr<Track>>();
    PTrack nextTrack = nextIndex.data(TrackRole).value<shared_ptr<Track>>();

    bool prevMatch = sameGroup(prevTrack, currentTrack);
    bool nextMatch = sameGroup(currentTrack, nextTrack);

    if (prevMatch) {
        if (nextMatch)
            mode = Grouping::Body;
        else
            mode = Grouping::Tail;
    } else {
        if (nextMatch)
            mode = Grouping::Head;
        else
            mode = Grouping::None;
    }
    groupingMode_.insert(index.row(), mode);
    return mode;
}

void PlaylistFilter::invalidateGrouping()
{
    groupingMode_.clear();
}

QVariant PlaylistFilter::data(const QModelIndex& index, int role) const
{
    if(!index.isValid())
        return QVariant();

    if (role == GroupingModeRole) {
        QVariant ret;
        ret.setValue(groupingMode(index));
        return ret;
    }

    return QSortFilterProxyModel::data(index, role);
}

bool PlaylistFilter::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    PTrack leftTrack = left.data(TrackRole).value<shared_ptr<Track>>();
    PTrack rightTrack = right.data(TrackRole).value<shared_ptr<Track>>();
    if (leftTrack->location == rightTrack->location && leftTrack->isCueTrack() && rightTrack->isCueTrack())
        return leftTrack->metadata["track"].toInt() < rightTrack->metadata["track"].toInt();
    return leftTrack->location < rightTrack->location;
}

#include "playlistfilter.moc"