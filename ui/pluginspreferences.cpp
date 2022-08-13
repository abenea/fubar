#include "pluginspreferences.h"
#include "pluginmanager.h"
#include <QCheckBox>
#include <QDebug>

PluginsPreferences::PluginsPreferences(QWidget *parent) : QDialog(parent) {
    setupUi(this);
    std::vector<QString> plugins = PluginManager::instance->getPlugins();
    pluginTable->setRowCount(plugins.size());
    pluginTable->setColumnCount(2);
    for (std::size_t i = 0; i < plugins.size(); ++i) {
        QCheckBox *checkbox = new QCheckBox();
        checkbox->setCheckState(PluginManager::instance->isEnabled(plugins[i]) ? Qt::Checked
                                                                               : Qt::Unchecked);
        pluginTable->setCellWidget(i, 0, checkbox);

        QTableWidgetItem *pluginName = new QTableWidgetItem(plugins[i]);
        pluginName->setFlags(Qt::ItemIsEnabled);
        pluginTable->setItem(i, 1, pluginName);
    }
    connect(pluginTable, SIGNAL(itemDoubleClicked(QTableWidgetItem *)), this,
            SLOT(slot_itemDoubleClicked(QTableWidgetItem *)));
}

void PluginsPreferences::slot_itemDoubleClicked(QTableWidgetItem *item) {
    PluginManager::instance->configurePlugin(item->text());
}

void PluginsPreferences::on_okButton_clicked() {
    // Loading plugins only once because qt4.8 is bugged
    // https://bugreports.qt-project.org/browse/QTBUG-24079
    // Checking/unchecking the checkbox calls init/deinit on the plugin
    // An advantage to having the plugins loaded at all times is that they are
    // configurable even when disabled.
    for (int i = 0; i < pluginTable->rowCount(); ++i) {
        QCheckBox *checkbox = (QCheckBox *)pluginTable->cellWidget(i, 0);
        bool enabled = checkbox->checkState() == Qt::Checked;
        QString plugin = pluginTable->item(i, 1)->text();
        if (enabled)
            PluginManager::instance->enablePlugin(plugin);
        else
            PluginManager::instance->disablePlugin(plugin);
    }
    QDialog::accept();
}
