#include "tabbar.h"
#include <QMouseEvent>
#include <QDebug>

TabBar::TabBar(QWidget *parent): QTabBar(parent), lineEdit_(nullptr), renamingIndex_(-1)
{
    installEventFilter(this);
    setAcceptDrops(true);
}

bool TabBar::eventFilter(QObject *obj, QEvent *event)
{
    bool result = QObject::eventFilter(obj, event);

    if (obj == this) {
        if (event->type() == QEvent::MouseButtonPress) {
            //see if the user is trying to move away from the editing by clicking another tab
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            int clickedTabId = tabAt(me->pos());
             if (renamingIndex_ != -1 && renamingIndex_ != clickedTabId)
                 editFinished();
            return false;
        }
        if (event->type() == QEvent::MouseButtonDblClick) {
            //perhaps we need to start a new name editing action...
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() != Qt::LeftButton)
                return result;
            int clickedTabId = tabAt(me->pos());
            if (clickedTabId == -1)
                return result;
            if (!isTabEnabled(clickedTabId))
                return result;
            triggerRename(clickedTabId);
            return true; //no further handling of this event is required
        }
    }

    //handle some events on the line edit to make it behave itself nicely as a rename editor
    if (obj == lineEdit_) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                lineEdit_->deleteLater();
                lineEdit_ = nullptr;
                return true; //no further handling of this event is required
            } else if (ke->key() == Qt::Key_Return) {
                editFinished();
            }
        }
    }

    return result;
}

void TabBar::triggerRename(int index)
{
    renamingIndex_ = index;
    lineEdit_ = new QLineEdit(this);
    lineEdit_->setText(tabText(index));
    lineEdit_->installEventFilter(this);
    lineEdit_->setGeometry(tabRect(index));
    lineEdit_->setCursorPosition(lineEdit_->text().size());
    lineEdit_->setFocus();
    lineEdit_->show();
}

void TabBar::editFinished()
{
    if (renamingIndex_ != -1 && lineEdit_) {
        setTabText(renamingIndex_, lineEdit_->text());
        lineEdit_->hide();
        lineEdit_->deleteLater();
        lineEdit_ = nullptr;
    }
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    int index = tabAt(event->pos());
    if (event->button() == Qt::MidButton) {
        event->accept();
        if (index != -1)
            emit tabCloseRequested(index);
        else
            emit newTabRequested();
    } else
        QTabBar::mousePressEvent(event);
}

void TabBar::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void TabBar::dragMoveEvent(QDragMoveEvent *event)
{
    int index = tabAt(event->pos());
    if (index == -1)
        emit newTabRequested();
    else
        setCurrentIndex(index);
}

#include "tabbar.moc"
