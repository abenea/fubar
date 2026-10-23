#include "ui/elidedlabel.h"

#include <QFontMetricsF>
#include <QTextDocument>
#include <QtMath>

ElidedLabel::ElidedLabel(int maxWidth, QWidget *parent) : QLabel(parent), maxWidth_(maxWidth) {
    setTextFormat(Qt::PlainText);
}

void ElidedLabel::setFullText(const QString &text) {
    fullText_ = text;
    // Tooltips that look like HTML are shown as rich text
    setToolTip(Qt::mightBeRichText(text) ? "<qt>" + text.toHtmlEscaped() + "</qt>" : text);
    elide();
    updateGeometry();
}

QSize ElidedLabel::sizeHint() const {
    QMargins margins = contentsMargins();
    // Rounded up: elidedText() elides a text wider than the width by a fraction of a pixel
    int width = qCeil(QFontMetricsF(font()).horizontalAdvance(fullText_)) + margins.left() +
                margins.right() + 2 * margin();
    return QSize(qMin(width, maxWidth_), QLabel::sizeHint().height());
}

QSize ElidedLabel::minimumSizeHint() const {
    return QSize(0, QLabel::minimumSizeHint().height());
}

void ElidedLabel::resizeEvent(QResizeEvent *event) {
    QLabel::resizeEvent(event);
    elide();
}

void ElidedLabel::elide() {
    int width = contentsRect().width() - 2 * margin();
    setText(fontMetrics().elidedText(fullText_, Qt::ElideMiddle, width));
}
