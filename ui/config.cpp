#include "config.h"
#include <QDebug>

void Config::set(QString key, QVariant value)
{
    config_[key] = value;
    emit keySet(key, value);
    qDebug() << key << "=" << value;
}

#include "config.moc"