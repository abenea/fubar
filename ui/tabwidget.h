#pragma once
#include <QTabWidget>

class TabBar;

class TabWidget : public QTabWidget
{
    Q_OBJECT
public:
    TabWidget(QWidget* parent = 0);

signals:
    void tabCloseRequested(int index);

protected slots:
    void slotTabCloseRequested(int index);

private:
    TabBar* tabBar_;
};