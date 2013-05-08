#pragma once
#include "ui/ui_configwindow.h"
#include <QDialog>

class Config;

class ConfigWindow : public QDialog, private Ui::ConfigWindow
{
    Q_OBJECT
public:
    ConfigWindow(Config& config, QWidget *parent = 0);

private slots:
    void cellChanged(int row, int column);

private:
    Config& config_;
};
