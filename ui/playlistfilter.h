#pragma once

#include "library/track_forward.h"
#include <QSortFilterProxyModel>
#include <QStringList>

namespace Grouping
{
    enum Mode
    {
        None = 1,
        Head,
        Body,
        Tail,
        Invalid
    };
};

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
    static bool sameGroup(PTrack track1, PTrack track2);

private slots:
    void invalidateGrouping();
};

Q_DECLARE_METATYPE(Grouping::Mode)
