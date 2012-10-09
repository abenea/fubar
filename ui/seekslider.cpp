#include "seekslider.h"
#include <QStyle>
#include <QStyleOptionSlider>
#include <QDebug>

SeekSlider::SeekSlider(Phonon::MediaObject* mediaObject, QWidget* parent)
    : QSlider(Qt::Horizontal, parent)
    , mediaObject_(mediaObject)
    , pressed_(false)
{
    QObject::connect(mediaObject, SIGNAL(tick(qint64)), this, SLOT(tick(qint64)));
    QObject::connect(this, SIGNAL(sliderPressed()), this, SLOT(sliderPressedAction()));
    QObject::connect(this, SIGNAL(sliderReleased()), this, SLOT(sliderReleasedAction()));
    setLimits(0, 0);
}

void SeekSlider::setLimits(int min, int max)
{
    setMinimum(min);
    setMaximum(max * 1000);
}

void SeekSlider::scrollTo(int pos)
{
    mediaObject_->seek(pos);
}

void SeekSlider::tick(qint64 pos)
{
    if (!pressed_)
        setValue(pos);
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

void SeekSlider::mousePressEvent(QMouseEvent* event)
{
    QSlider::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
//         QStyleOptionSlider option;
//         initStyleOption(&option);
//         QRect rect = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
        int value = QStyle::sliderValueFromPosition(minimum(), maximum(), event->x(), width());
        qDebug() << "SeekSlider::mouseMoveEvent() " << value;
        setValue(value);
        scrollTo(value);
    }
}

#include "seekslider.moc"