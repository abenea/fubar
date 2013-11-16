#include "tabwidget.h"
#include "tabbar.h"
#include "playlisttab.h"
#include <QDebug>

TabWidget::TabWidget(QWidget *parent): QTabWidget(parent)
{
    tabBar_ = new TabBar(this);
    QObject::connect(tabBar_, SIGNAL(tabCloseRequested(int)), this, SLOT(slotTabCloseRequested(int)));
    QObject::connect(tabBar_, SIGNAL(newTabRequested()), this, SLOT(slotNewTabRequested()));
    QObject::connect(tabBar_, SIGNAL(setCurrentRequested(int)), this, SLOT(slotSetCurrentRequested(int)));
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

void TabWidget::slotSetCurrentRequested(int index)
{
    const PlaylistTab* tab = qobject_cast<const PlaylistTab*>(widget(index));
    if (tab && tab->isEditable())
        setCurrentIndex(index);
}

#include "tabwidget.moc"
