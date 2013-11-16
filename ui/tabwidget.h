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
    void newTabRequested();

protected slots:
    void slotTabCloseRequested(int index);
    void slotNewTabRequested();

private:
    TabBar* tabBar_;
};