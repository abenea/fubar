#include "ui/config.h"

#include <QDebug>

void Config::set(QString key, QVariant value) {
    config_[key] = value;
    emit keySet(key, value);
}
