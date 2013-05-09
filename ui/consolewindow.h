#pragma once

#include "ui/ui_console.h"
#include <QDialog>
#include <deque>

class ConsoleWindow : public QDialog, private Ui::Console
{
    Q_OBJECT
public:
    ConsoleWindow(QWidget *parent = 0);

public slots:
    void show();
    void hide();

private slots:
    void updated(QtMsgType type, QString message);

private:
    void setCursorToEnd();

    std::deque<std::pair<QtMsgType, QString>> messages_;
    QByteArray geometry_;
};
