#ifndef SEEKSLIDER_H
#define SEEKSLIDER_H

#include <QMouseEvent>
#include <QSlider>

class AudioOutput;

class SeekSlider : public QSlider
{
    Q_OBJECT
public:
    explicit SeekSlider(AudioOutput* mediaObject, QWidget* parent = 0);

    void setLimits(int min, int max);
    void scrollTo(int pos);

signals:
    void movedByUser(int position);

private slots:
    void tick(qint64 pos);
    void sliderPressedAction();
    void sliderReleasedAction();

private:
    void mousePressEvent(QMouseEvent *event);
    inline int pick(const QPoint &pt) const;
    int pixelPosToRangeValue(int pos) const;

    AudioOutput* mediaObject_;
    bool pressed_;
};

#endif // SEEKSLIDER_H
