#pragma once
#include "ui/ui_configwindow.h"
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

class Config;

class ConfigFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    void setFilter(QString filter);

private:
    QStringList filter_;
};

class ConfigWindow : public QDialog, private Ui::ConfigWindow {
    Q_OBJECT
public:
    ConfigWindow(Config &config, QWidget *parent = 0);

private slots:
    void itemChanged(QStandardItem *item);
    void changedFilter(QString filter);

private:
    QStandardItemModel model_;
    ConfigFilter configFilter_;
    Config &config_;
};
