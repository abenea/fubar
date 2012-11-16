#include "preferences.h"
#include <QSettings>

Preferences::Preferences(LastfmPlugin& plugin, QWidget* parent)
    : QDialog(parent)
    , plugin_(plugin)
{
    setupUi(this);
    QSettings settings;
    old_username_ = settings.value("plugins/lastfm/username").toString();
    old_password_ = settings.value("plugins/lastfm/password").toString();
    usernameField->setText(old_username_);
    passwordField->setText(old_password_);
}

void Preferences::on_saveButton_clicked()
{
    if (old_username_ != usernameField->text() || old_password_ != passwordField->text()) {
        QSettings settings;
        settings.setValue("plugins/lastfm/username", usernameField->text());
        settings.setValue("plugins/lastfm/password", passwordField->text());
        plugin_.logout();
        plugin_.login();
    }
    QDialog::accept();
}

#include "preferences.moc"