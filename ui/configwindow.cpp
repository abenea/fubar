#include "ui/configwindow.h"

#include "ui/ui_configwindow.h"

ConfigWindow::ConfigWindow(Config &config, QWidget *parent)
    : QDialog(parent), ui_(new Ui::ConfigWindow), config_(config) {
    ui_->setupUi(this);
    ui_->tableView->verticalHeader()->hide();
    ui_->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    model_.setHorizontalHeaderLabels({"Preference name", "Type", "Value"});
    for (auto &kv : config_.config_) {
        // name
        auto name_item = new QStandardItem(kv.first);
        name_item->setFlags(Qt::NoItemFlags);
        // type
        QString type = QString(kv.second.typeName()).toLower();
        if (type.startsWith("q"))
            type.remove(0, 1);
        auto type_item = new QStandardItem(type);
        type_item->setFlags(Qt::NoItemFlags);
        // value
        auto value_item = new QStandardItem();
        value_item->setData(kv.second, Qt::EditRole);
        value_item->setWhatsThis(kv.first);
        model_.appendRow({name_item, type_item, value_item});
    }
    configFilter_.setSourceModel(&model_);
    ui_->tableView->setModel(&configFilter_);
    connect(&model_, SIGNAL(itemChanged(QStandardItem *)), this,
            SLOT(itemChanged(QStandardItem *)));
    connect(ui_->filter, SIGNAL(textChanged(QString)), this, SLOT(changedFilter(QString)));
}

ConfigWindow::~ConfigWindow() {}

void ConfigWindow::changedFilter(QString filter) { configFilter_.setFilter(filter); }

void ConfigWindow::itemChanged(QStandardItem *item) {
    config_.set(item->whatsThis(), item->data(Qt::EditRole));
}

bool ConfigFilter::filterAcceptsRow(int source_row, const QModelIndex & /*source_parent*/) const {
    if (filter_.empty())
        return true;
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(sourceModel());
    if (!model)
        return true;
    QStandardItem *item = model->item(source_row, 2);
    if (!item)
        return true;
    for (const auto &keyword : filter_) {
        if (!item->whatsThis().contains(keyword))
            return false;
    }
    return true;
}

void ConfigFilter::setFilter(QString filter) {
    filter_ = filter.split(" ", Qt::SkipEmptyParts);
    invalidateFilter();
}
