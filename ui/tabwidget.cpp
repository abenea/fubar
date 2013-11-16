#include "tabwidget.h"
#include "tabbar.h"
#include <QDebug>

TabWidget::TabWidget(QWidget *parent): QTabWidget(parent)
{
    tabBar_ = new TabBar(this);
    QObject::connect(tabBar_, SIGNAL(tabCloseRequested(int)), this, SLOT(slotTabCloseRequested(int)));
    QObject::connect(tabBar_, SIGNAL(newTabRequested()), this, SLOT(slotNewTabRequested()));
    setTabBar(tabBar_);
}

void TabWidget::slotTabCloseRequested(int index)
{
    emit tabCloseRequested(index);
}

void TabWidget::slotNewTabRequested()
{
    emit newTabRequested();
}

#include "tabwidget.moc"
