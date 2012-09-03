#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QStatusBar>

class StatusBar : public QStatusBar
{
    Q_OBJECT
public:
    StatusBar(QWidget *parent);

protected:
    void mouseDoubleClickEvent(QMouseEvent * event);

signals:
    void statusBarDoubleClicked();
};

#endif // STATUSBAR_H
