#ifndef PLUGINSPREFERENCES_H
#define PLUGINSPREFERENCES_H

#include "ui/ui_plugins.h"
#include <QDialog>

class QTableWidgetItem;

class PluginsPreferences : public QDialog, private Ui::Plugins
{
    Q_OBJECT
public:
    PluginsPreferences(QWidget *parent = 0);

private slots:
    void on_okButton_clicked();
    void on_itemDoubleClicked(QTableWidgetItem* item);
};

#endif // PLUGINSPREFERENCES_H
