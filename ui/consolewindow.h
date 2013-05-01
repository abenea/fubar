#pragma once

#include "ui/ui_console.h"
#include <QDialog>

class ConsoleWindow : public QDialog, private Ui::Console
{
    Q_OBJECT
public:
    ConsoleWindow(QWidget *parent = 0);

public slots:
    void show();

private slots:
    void updated();
};
