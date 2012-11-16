#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "plugins/lastfm/ui_config.h"
#include "plugins/lastfm/lastfmplugin.h"
#include <QDialog>

class Preferences : public QDialog, private Ui::PreferencesDialog
{
    Q_OBJECT
public:
    Preferences(LastfmPlugin& plugin, QWidget *parent = 0);

private slots:
    void on_saveButton_clicked();

private:
    LastfmPlugin& plugin_;
    QString old_username_;
    QString old_password_;
};

#endif // PREFERENCES_H
