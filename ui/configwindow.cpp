#include "configwindow.h"
#include "config.h"
#include <QDebug>

ConfigWindow::ConfigWindow(Config& config, QWidget* parent)
    : QDialog(parent),
    config_(config)
{
    setupUi(this);
	tableWidget->setColumnWidth(0, 300);
	tableWidget->setColumnWidth(1, 100);
	tableWidget->setColumnWidth(2, 300);
    tableWidget->setRowCount(config_.config_.size());
    int i = 0;
    for (auto& kv : config_.config_) {
        // name
        auto item = new QTableWidgetItem(kv.first);
        item->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(i, 0, item);

        // type
        item = new QTableWidgetItem(QTableWidgetItem::UserType);
        item->setFlags(Qt::NoItemFlags);
        QString type;
        switch (kv.second.type()) {
            case QVariant::Bool:
                type = "Boolean";
                break;
            case QVariant::Int:
                type = "Integer";
                break;
            case QVariant::String:
                type = "String";
                break;
            default:
                type = "Unknown";
        }
        item->setData(Qt::DisplayRole, type);
        tableWidget->setItem(i, 1, item);

        // value
        item = new QTableWidgetItem(QTableWidgetItem::UserType);
        item->setData(Qt::DisplayRole, kv.second);
        tableWidget->setItem(i, 2, item);
    }
    QObject::connect(tableWidget, SIGNAL(cellChanged(int, int)), this, SLOT(cellChanged(int,int)));
}

void ConfigWindow::cellChanged(int row, int column)
{
    if (column != 2)
        return;
    config_.set(tableWidget->item(row, 0)->text(), tableWidget->item(row, 2)->data(Qt::DisplayRole));
}

#include "configwindow.moc"
