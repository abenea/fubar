#pragma once

#include "ui/audioplayer.h"
#include <QMouseEvent>
#include <QSlider>

class SeekSlider : public QSlider {
    Q_OBJECT
public:
    explicit SeekSlider(AudioPlayer &player, QWidget *parent = 0);

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

    AudioPlayer &player_;
    bool pressed_;
};
