#pragma once

#include "ui/playlistfilter.h"
#include <QStyledItemDelegate>

class PlaylistItemDelegate : public QStyledItemDelegate {
public:
    PlaylistItemDelegate(QObject *parent = 0) : QStyledItemDelegate(parent) {}

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    virtual void
    paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    static Grouping::Mode getGroupMode(const QModelIndex &index);
};
