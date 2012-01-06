#ifndef PLAYLISTFILTER_H
#define PLAYLISTFILTER_H

#include <QSortFilterProxyModel>
#include <QStringList>
#include "playlistmodel.h"

class PlaylistFilter : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    PlaylistFilter(QObject *parent = 0);

    void setFilter(const QString &filter);
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const;

private:
    QStringList filter_;
    mutable QHash<int, Grouping::Mode> groupingMode_;

    Grouping::Mode groupingMode(const QModelIndex& index) const;
    static bool sameGroup(boost::shared_ptr<Track> track1, boost::shared_ptr<Track> track2);

private slots:
    void invalidateGrouping();
};

#endif // PLAYLISTFILTER_H
