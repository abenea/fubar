#pragma once

#include <QDialog>

class QTableWidgetItem;

namespace Ui {
class Plugins;
}

class PluginsPreferences : public QDialog {
    Q_OBJECT
public:
    PluginsPreferences(QWidget *parent = 0);
    ~PluginsPreferences();

private slots:
    void on_okButton_clicked();
    void slot_itemDoubleClicked(QTableWidgetItem *item);

private:
    std::unique_ptr<Ui::Plugins> ui_;
};
