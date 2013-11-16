#pragma once
#include <QTabBar>
#include <QLineEdit>

class QMouseEvent;

class TabBar : public QTabBar
{
    Q_OBJECT
public:
    TabBar(QWidget * parent = 0);

signals:
    void tabCloseRequested(int index);
    void newTabRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void triggerRename(int index);
    void editFinished();

private:
    QLineEdit* lineEdit_;
    int renamingIndex_;
};
