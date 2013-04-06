#include "seekslider.h"
#include "../audiooutput.h"
#include <QStyle>
#include <QStyleOptionSlider>
#include <QDebug>

SeekSlider::SeekSlider(AudioOutput* mediaObject, QWidget* parent)
    : QSlider(Qt::Horizontal, parent)
    , mediaObject_(mediaObject)
    , pressed_(false)
{
    QObject::connect(mediaObject_, SIGNAL(tick(qint64)), this, SLOT(tick(qint64)));
    QObject::connect(this, SIGNAL(sliderPressed()), this, SLOT(sliderPressedAction()));
    QObject::connect(this, SIGNAL(sliderReleased()), this, SLOT(sliderReleasedAction()));
    setLimits(0, 0);
}

void SeekSlider::setLimits(int min, int max)
{
    setValue(0);
    setMinimum(min);
    setMaximum(max * 1000);
}

void SeekSlider::scrollTo(int pos)
{
    mediaObject_->seek(pos);
    emit movedByUser(pos);
}

void SeekSlider::tick(qint64 pos)
{
    if (!pressed_) {
        setValue(pos);
    }
}

void SeekSlider::sliderPressedAction()
{
    pressed_ = true;
}

void SeekSlider::sliderReleasedAction()
{
    scrollTo(value());
    pressed_ = false;
}

/*
 * Copypasta from Phonon::SwiftSlider
 */

// Function copied from qslider.cpp
inline int SeekSlider::pick(const QPoint &pt) const
{
    return orientation() == Qt::Horizontal ? pt.x() : pt.y();
}

// Function copied from qslider.cpp and modified to make it compile
int SeekSlider::pixelPosToRangeValue(int pos) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect gr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    QRect sr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    int sliderMin, sliderMax, sliderLength;

    if (orientation() == Qt::Horizontal) {
        sliderLength = sr.width();
        sliderMin = gr.x();
        sliderMax = gr.right() - sliderLength + 1;
    } else {
        sliderLength = sr.height();
        sliderMin = gr.y();
        sliderMax = gr.bottom() - sliderLength + 1;
    }
    return QStyle::sliderValueFromPosition(minimum(), maximum(), pos - sliderMin,
                                           sliderMax - sliderMin, opt.upsideDown);
}

// Based on code from qslider.cpp
void SeekSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
        const QPoint center = sliderRect.center() - sliderRect.topLeft();
        // to take half of the slider off for the setSliderPosition call we use the center - topLeft

        if (!sliderRect.contains(event->pos())) {
            event->accept();

            int pos = pixelPosToRangeValue(pick(event->pos() - center));
            setSliderPosition(pos);
            scrollTo(pos);
            triggerAction(SliderMove);
            setRepeatAction(SliderNoAction);
        } else {
            QSlider::mousePressEvent(event);
        }
    } else {
        QSlider::mousePressEvent(event);
    }
}

#include "seekslider.moc"