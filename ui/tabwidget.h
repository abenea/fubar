#pragma once
#include <QTabWidget>

class TabBar;
class QMimeData;

class TabWidget : public QTabWidget {
    Q_OBJECT
public:
    TabWidget(QWidget *parent = 0);

signals:
    void tabCloseRequested(int index);
    void newTabRequested();

protected slots:
    void slotTabCloseRequested(int index);
    void slotNewTabRequested();
    void slotSetCurrentRequested(int index);
    void slotDropRequested(int index, const QMimeData *mimeData);

private:
    TabBar *tabBar_;
};