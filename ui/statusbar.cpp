#include "ui/statusbar.h"

StatusBar::StatusBar(QWidget *parent) : QStatusBar(parent) {}

void StatusBar::mouseDoubleClickEvent(QMouseEvent *event) {
    QWidget::mouseDoubleClickEvent(event);
    emit statusBarDoubleClicked();
}
