#ifndef PLUGINSPREFERENCES_H
#define PLUGINSPREFERENCES_H

#include "ui/ui_plugins.h"
#include <QDialog>


class PluginsPreferences : public QDialog, private Ui::Plugins
{
    Q_OBJECT
public:
    PluginsPreferences(QWidget *parent = 0);

private slots:
    void on_okButton_clicked();
};

#endif // PLUGINSPREFERENCES_H
