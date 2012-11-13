#include "pluginspreferences.h"
#include "../plugins/pluginmanager.h"
#include <QDebug>

PluginsPreferences::PluginsPreferences(QWidget* parent)
    : QDialog(parent)
{
    setupUi(this);
    std::vector<QString> plugins = PluginManager::instance->getPlugins();
    pluginTable->setRowCount(plugins.size());
    pluginTable->setColumnCount(2);
    for (std::size_t i = 0; i < plugins.size(); ++i) {
        pluginTable->setItem(i, 1, new QTableWidgetItem(plugins[i]));
        QTableWidgetItem* checkbox = new QTableWidgetItem();
        checkbox->setFlags(Qt::NoItemFlags/*Qt::ItemIsUserCheckable | Qt::ItemIsEnabled*/);
        checkbox->setCheckState(Qt::Unchecked);
        pluginTable->setItem(i, 0, checkbox);
    }
}

void PluginsPreferences::on_okButton_clicked()
{
    // No enabling/disabling plugins yet cause qt4.8 is bugged
    // https://bugreports.qt-project.org/browse/QTBUG-24079
//     for (int i = 0; i < pluginTable->rowCount(); ++i) {
//         bool enabled = pluginTable->item(i, 0)->checkState() == Qt::Checked;
//         QString plugin = pluginTable->item(i, 1)->text();
//         if (enabled)
//             PluginManager::instance->enablePlugin(plugin);
//         else
//             PluginManager::instance->disablePlugin(plugin);
//     }
    QDialog::accept();
}

#include "pluginspreferences.moc"
