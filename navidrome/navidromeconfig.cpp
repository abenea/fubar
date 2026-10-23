#include "navidrome/navidromeconfig.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

bool NavidromeConfig::isComplete() const {
    return !serverUrl.isEmpty() && !username.isEmpty() && !salt.isEmpty() && !token.isEmpty();
}

bool NavidromeConfig::sameConnection(const NavidromeConfig &other) const {
    return sameAccount(other) && salt == other.salt && token == other.token;
}

bool NavidromeConfig::sameAccount(const NavidromeConfig &other) const {
    return serverUrl == other.serverUrl && username == other.username;
}

void NavidromeConfig::setPassword(const QString &password) {
    QRandomGenerator *random = QRandomGenerator::system();
    salt = QStringLiteral("%1%2")
               .arg(random->generate64(), 16, 16, QLatin1Char('0'))
               .arg(random->generate64(), 16, 16, QLatin1Char('0'));
    token = makeToken(password, salt);
}

QString NavidromeConfig::makeToken(const QString &password, const QString &salt) {
    return QString::fromLatin1(
        QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Md5).toHex());
}

QString NavidromeConfig::normalizeServerUrl(const QString &url) {
    static const QRegularExpression scheme(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*://"));
    static const QRegularExpression authorityEnd(QStringLiteral("[/?#]"));
    QString input = url.trimmed();
    if (input.isEmpty())
        return input;
    QRegularExpressionMatch match = scheme.match(input);
    QUrl parsed(match.hasMatch() ? input : QStringLiteral("http://") + input);
    if (!parsed.isValid() || parsed.host().isEmpty()) {
        // Kept as is, except for the user info.
        int start = match.hasMatch() ? match.capturedEnd() : 0;
        int end = input.indexOf(authorityEnd, start);
        int at = input.mid(start, end < 0 ? -1 : end - start).lastIndexOf('@');
        return at < 0 ? input : normalizeServerUrl(input.remove(start, at + 1));
    }
    parsed = parsed.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
    QString path = parsed.path(QUrl::FullyEncoded);
    while (path.endsWith('/') || path.endsWith(QStringLiteral("/rest")))
        path.chop(path.endsWith('/') ? 1 : 5);
    parsed.setPath(path, QUrl::TolerantMode);
    QString result = parsed.toString(QUrl::FullyEncoded);
    return result.isEmpty() ? input : result;
}

bool NavidromeConfig::isValidServerUrl(const QString &url) {
    QUrl parsed(url);
    return parsed.isValid() && (parsed.scheme() == "http" || parsed.scheme() == "https") &&
           !parsed.host().isEmpty();
}

NavidromeConfig NavidromeConfig::load(QSettings &settings) {
    NavidromeConfig config;
    config.serverUrl = normalizeServerUrl(settings.value("navidrome/serverUrl").toString());
    config.username = settings.value("navidrome/username").toString();
    config.salt = settings.value("navidrome/salt").toString();
    config.token = settings.value("navidrome/token").toString();
    config.scrobble = settings.value("navidrome/scrobble", true).toBool();
    return config;
}

void NavidromeConfig::save(QSettings &settings) const {
    settings.setValue("navidrome/serverUrl", serverUrl);
    settings.setValue("navidrome/username", username);
    settings.setValue("navidrome/salt", salt);
    settings.setValue("navidrome/token", token);
    settings.setValue("navidrome/scrobble", scrobble);
    // The token is as good as the password. Later rewrites of the file keep its mode.
    settings.sync();
    if (QFile::exists(settings.fileName()))
        QFile::setPermissions(settings.fileName(), QFile::ReadOwner | QFile::WriteOwner);
}
