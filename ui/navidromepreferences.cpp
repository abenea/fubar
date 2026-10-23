#include "ui/navidromepreferences.h"

#include "navidrome/navidromeclient.h"
#include "ui/ui_navidromepreferences.h"
#include <QHostAddress>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>

namespace {

bool isLocalAddress(const QHostAddress &address) {
    bool isIPv4 = false;
    quint32 ipv4 = address.toIPv4Address(&isIPv4);
    QHostAddress host = isIPv4 ? QHostAddress(ipv4) : address;
    for (const char *subnet : {"127.0.0.0/8", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16",
                               "169.254.0.0/16", "::1/128", "fc00::/7", "fe80::/10"}) {
        if (host.isInSubnet(QHostAddress::parseSubnet(subnet)))
            return true;
    }
    return false;
}

bool isLocalHostName(const QString &host) {
    if (host == "localhost")
        return true;
    for (const char *suffix : {".localhost", ".local", ".home.arpa", ".internal"}) {
        if (host.endsWith(QLatin1String(suffix)))
            return true;
    }
    return false;
}

} // namespace

NavidromePreferences::NavidromePreferences(NavidromeClient &client, QWidget *parent)
    : QDialog(parent), ui_(new Ui::NavidromePreferences), client_(client) {
    ui_->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    for (QLabel *hint : {ui_->insecureWarning, ui_->scrobbleHint}) {
        QFont font = hint->font();
        if (font.pointSizeF() > 0) {
            font.setPointSizeF(font.pointSizeF() * 0.9);
            hint->setFont(font);
        }
    }
    const NavidromeConfig &config = client_.config();
    ui_->serverUrl->setText(config.serverUrl);
    ui_->username->setText(config.username);
    ui_->scrobble->setChecked(config.scrobble);
    formChanged();
    for (QLineEdit *edit : {ui_->serverUrl, ui_->username, ui_->password})
        connect(edit, &QLineEdit::textChanged, this, &NavidromePreferences::formChanged);
}

NavidromePreferences::~NavidromePreferences() {}

bool NavidromePreferences::canReuseToken(const NavidromeConfig &saved,
                                         const QString &serverUrl,
                                         const QString &username) {
    return saved.isComplete() &&
           saved.serverUrl == NavidromeConfig::normalizeServerUrl(serverUrl) &&
           saved.username == username.trimmed();
}

bool NavidromePreferences::isUnencrypted(const QString &serverUrl) {
    QUrl url(NavidromeConfig::normalizeServerUrl(serverUrl));
    if (url.scheme() != "http" || url.host().isEmpty())
        return false;
    QHostAddress address;
    if (address.setAddress(url.host()))
        return !isLocalAddress(address);
    return !isLocalHostName(url.host());
}

QString NavidromePreferences::configFromForm(const NavidromeConfig &saved,
                                             const QString &serverUrl,
                                             const QString &username,
                                             const QString &password,
                                             bool scrobble,
                                             NavidromeConfig *config) {
    *config = NavidromeConfig();
    config->scrobble = scrobble;
    if (serverUrl.trimmed().isEmpty())
        return QString();
    config->serverUrl = NavidromeConfig::normalizeServerUrl(serverUrl);
    if (!NavidromeConfig::isValidServerUrl(config->serverUrl))
        return "Enter a valid http:// or https:// server URL";
    config->username = username.trimmed();
    if (config->username.isEmpty())
        return "Enter the username";
    if (!password.isEmpty()) {
        config->setPassword(password);
    } else if (canReuseToken(saved, serverUrl, username)) {
        config->salt = saved.salt;
        config->token = saved.token;
    } else {
        return "Enter the password";
    }
    return QString();
}

void NavidromePreferences::accept() {
    NavidromeConfig config;
    QString error = formConfig(&config);
    if (!error.isEmpty()) {
        showStatus(error);
        return;
    }
    NavidromeConfig previous = client_.config();
    QSettings settings;
    config.save(settings);
    client_.setConfig(config);
    emit configSaved(config.isComplete() &&
                     (!previous.isComplete() || !previous.sameAccount(config)));
    QDialog::accept();
}

void NavidromePreferences::on_testConnection_clicked() {
    cancelTest();
    NavidromeConfig config;
    QString error = formConfig(&config);
    if (error.isEmpty() && !config.isComplete())
        error = "Enter the server URL";
    if (!error.isEmpty()) {
        showStatus(error);
        return;
    }
    testContext_ = new QObject(this);
    ui_->testConnection->setEnabled(false);
    showStatus("Connecting…");
    client_.get(config, "ping", {}, testContext_,
                [this](const QJsonObject &response, const QString &error) {
                    testContext_->deleteLater();
                    ui_->testConnection->setEnabled(true);
                    if (error.isEmpty()) {
                        QString version = response.value("serverVersion").toString();
                        showStatus(QString("Connected to Navidrome %1").arg(version).trimmed());
                    } else {
                        showStatus("Connection failed: " + error);
                    }
                });
}

void NavidromePreferences::formChanged() {
    cancelTest();
    showStatus(QString());
    QString serverUrl = ui_->serverUrl->text();
    ui_->insecureWarning->setVisible(isUnencrypted(serverUrl));
    bool reusable = canReuseToken(client_.config(), serverUrl, ui_->username->text());
    ui_->password->setPlaceholderText(reusable ? "Unchanged" : QString());
}

QString NavidromePreferences::formConfig(NavidromeConfig *config) const {
    return configFromForm(client_.config(), ui_->serverUrl->text(), ui_->username->text(),
                          ui_->password->text(), ui_->scrobble->isChecked(), config);
}

void NavidromePreferences::cancelTest() {
    delete testContext_;
    ui_->testConnection->setEnabled(true);
}

void NavidromePreferences::showStatus(const QString &text) { ui_->status->setText(text); }
