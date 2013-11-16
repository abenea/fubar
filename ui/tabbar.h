#pragma once
#include <QTabBar>
#include <QLineEdit>

class QMouseEvent;
class QDragMoveEvent;

class TabBar : public QTabBar
{
    Q_OBJECT
public:
    TabBar(QWidget * parent = 0);

signals:
    void tabCloseRequested(int index);
    void newTabRequested();
    void setCurrentRequested(int index);

protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void triggerRename(int index);
    void editFinished();
    void dragEnterEvent(QDragEnterEvent* event);
    void dragMoveEvent(QDragMoveEvent* event);

private:
    QLineEdit* lineEdit_;
    int renamingIndex_;
};
