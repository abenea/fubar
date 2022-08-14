#pragma once

#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

#include "ui/config.h"

namespace Ui {
class ConfigWindow;
}

class ConfigFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    void setFilter(QString filter);

private:
    QStringList filter_;
};

class ConfigWindow : public QDialog {
    Q_OBJECT
public:
    ConfigWindow(Config &config, QWidget *parent = 0);
    virtual ~ConfigWindow();

private slots:
    void itemChanged(QStandardItem *item);
    void changedFilter(QString filter);

private:
    std::unique_ptr<Ui::ConfigWindow> ui_;
    QStandardItemModel model_;
    ConfigFilter configFilter_;
    Config &config_;
};
