#pragma once

#include "navidrome/navidromeconfig.h"

#include <QDialog>
#include <QPointer>
#include <memory>

class NavidromeClient;

namespace Ui {
class NavidromePreferences;
}

// Modeless dialog editing the Navidrome connection; deletes itself when closed. OK saves to
// QSettings and applies the config to `client`, which must outlive the dialog.
class NavidromePreferences : public QDialog {
    Q_OBJECT
public:
    explicit NavidromePreferences(NavidromeClient &client, QWidget *parent = nullptr);
    ~NavidromePreferences() override;

    // Rules of the form. They take the raw field values: URLs are normalized and usernames
    // trimmed before use.

    // True if the token of `saved` can be used for `serverUrl` and `username` without asking
    // for the password again.
    static bool canReuseToken(const NavidromeConfig &saved,
                              const QString &serverUrl,
                              const QString &username);
    // True if the URL is http:// (the default) and its host is not local: localhost, private,
    // link-local or loopback addresses, or names under .localhost, .local, .home.arpa, .internal.
    static bool isUnencrypted(const QString &serverUrl);
    // The config described by the form; an empty server URL gives an empty config. An empty
    // password keeps the salt and token of `saved` if canReuseToken. Returns "" on success, else
    // the reason the form is not acceptable.
    static QString configFromForm(const NavidromeConfig &saved,
                                  const QString &serverUrl,
                                  const QString &username,
                                  const QString &password,
                                  bool scrobble,
                                  NavidromeConfig *config);

public slots:
    void accept() override;

signals:
    // Emitted when OK saved the config. `accountChanged`: the client is configured now and
    // either was not before or was configured for another server or username.
    void configSaved(bool accountChanged);

private slots:
    void on_testConnection_clicked();
    void formChanged();

private:
    QString formConfig(NavidromeConfig *config) const;
    void cancelTest();
    void showStatus(const QString &text);

    std::unique_ptr<Ui::NavidromePreferences> ui_;
    NavidromeClient &client_;
    // Parent of the running connection test; deleting it drops the reply.
    QPointer<QObject> testContext_;
};
