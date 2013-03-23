#include "playlistitemdelegate.h"
#include "playlistmodel.h"
#include "mainwindow.h"
#include <QPainter>
#include <QApplication>

QSize PlaylistItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    int height = 0;

    QFont boldfont( option.font );
    boldfont.setBold( true );
    QFontMetricsF bfm( boldfont );

    int fontHeight = bfm.height();

    int rowCount = (getGroupMode(index) == Grouping::Head  || getGroupMode(index) == Grouping::None) ? 2 : 1;

    height = rowCount * fontHeight + rowCount * 2;
    return QSize(120, height);
}

void drawText(QPainter *painter, QRect &rect, const QString &text, int space = 0)
{
    painter->drawText(rect, text);
    QRect used = painter->boundingRect(rect, 0, text);
    rect.setLeft(rect.left() + used.width() + space);
}

void PlaylistItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    std::shared_ptr<Track> track = index.data(TrackRole).value<std::shared_ptr<Track> >();
    if (!track) {
        painter->restore();
        return;
    }

    QFontMetricsF nfm(option.font);
    QStyleOptionViewItem trackOption(option);
    trackOption.rect.setHeight(nfm.height());

    Grouping::Mode groupMode = getGroupMode(index);
    if (groupMode == Grouping::Head || groupMode == Grouping::None) {
        QStyleOptionViewItem headOption(option);
        headOption.rect.setHeight(nfm.height());
        headOption.rect.moveTop(headOption.rect.top() + 1);
        headOption.rect.setLeft(headOption.rect.left() + 2);

        headOption.font.setBold(true);
        painter->setFont(headOption.font);
        QRect remaining = headOption.rect;
        QString albumArtist = track->metadata.value("album artist", "");
        if (albumArtist.isEmpty())
            albumArtist = track->metadata["artist"];
        drawText(painter, remaining, albumArtist + " - ", 5);

        if (track->metadata["year"].toInt()) {
            headOption.font.setBold(false);
            painter->setFont(headOption.font);
            drawText(painter, remaining, QString("[" + track->metadata["year"] + "]"), 5);
        }

        headOption.font.setBold(true);
        painter->setFont(headOption.font);
        QString album = track->metadata["album"];
        if (album.isEmpty())
            album = "?";
        drawText(painter, remaining, album, 5);

        trackOption.rect.moveTop(headOption.rect.bottom() + 2);
    }

    // Paint the track
    QStyleOptionViewItemV4 back(option);
    back.rect = trackOption.rect;
    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &back, painter);

    QRect leftTextRect = trackOption.rect;
    leftTextRect.setLeft(trackOption.rect.left() + 100);
    leftTextRect.setRight(trackOption.rect.right() - 50);
    QRect rightTextRect = trackOption.rect;
    rightTextRect.setLeft(leftTextRect.right());

    painter->setFont(trackOption.font);
    QFontMetricsF tfm(trackOption.font);
    QString title = track->metadata["title"];
    QString albumArtist = track->metadata.value("album artist", "");
    if (!albumArtist.isEmpty() && albumArtist != track->metadata["artist"] && !track->metadata["artist"].isEmpty())
        title += " // " + track->metadata["artist"];
    QString leftText = QString("%1. %2").arg(track->metadata["track"], 2, QChar('0')).arg(title);
    int minutes = track->audioproperties.length / 60;
    int seconds = track->audioproperties.length % 60;
    QString rightText = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
    painter->drawText(leftTextRect, tfm.elidedText(leftText, Qt::ElideRight, leftTextRect.width()));
    painter->drawText(rightTextRect, Qt::AlignRight, rightText);

    QRect queueRect = trackOption.rect;
    queueRect.setLeft(trackOption.rect.left() + 45);
    queueRect.setRight(trackOption.rect.left() + 55);
    painter->drawText(queueRect, Qt::AlignHCenter, MainWindow::instance->queue.isQueued(MainWindow::instance->getActivePlaylist(), track) ? QString(QChar(1342)) : QString());

    painter->restore();
}

Grouping::Mode PlaylistItemDelegate::getGroupMode(const QModelIndex& index)
{
    return index.data(GroupingModeRole).value<Grouping::Mode>();
}

