#ifndef SEEKSLIDER_H
#define SEEKSLIDER_H

#include <MediaObject>
#include <QMouseEvent>
#include <QSlider>

class SeekSlider : public QSlider
{
    Q_OBJECT
public:
    explicit SeekSlider(Phonon::MediaObject* mediaObject, QWidget* parent = 0);

    void setLimits(int min, int max);
    void scrollTo(int pos);

private slots:
    void tick(qint64 pos);
    void sliderPressedAction();
    void sliderReleasedAction();

private:
    void mousePressEvent(QMouseEvent *event);
    inline int pick(const QPoint &pt) const;
    int pixelPosToRangeValue(int pos) const;

    Phonon::MediaObject* mediaObject_;
    bool pressed_;
};

#endif // SEEKSLIDER_H
