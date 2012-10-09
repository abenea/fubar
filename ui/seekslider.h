#ifndef SEEKSLIDER_H
#define SEEKSLIDER_H

#include <QSlider>
#include <MediaObject>
#include <QMouseEvent>

class SeekSlider : public QSlider
{
    Q_OBJECT
public:
    explicit SeekSlider(Phonon::MediaObject* mediaObject, QWidget* parent = 0);

    void setLimits(int min, int max);
    void scrollTo(int pos);

protected:
    void mousePressEvent(QMouseEvent* event);

private slots:
    void tick(qint64 pos);
    void sliderPressedAction();
    void sliderReleasedAction();

private:
    Phonon::MediaObject* mediaObject_;
    bool pressed_;
};

#endif // SEEKSLIDER_H
