#pragma once

#include <QString>

class QSettings;

struct NavidromeConfig {
    QString serverUrl;
    QString username;
    QString salt;
    QString token;
    bool scrobble = true;

    bool isComplete() const;
    bool sameConnection(const NavidromeConfig &other) const;
    bool sameAccount(const NavidromeConfig &other) const;
    void setPassword(const QString &password);

    static QString makeToken(const QString &password, const QString &salt);
    // Unparsable input is returned trimmed and without user info but otherwise unchanged; check it
    // with isValidServerUrl.
    static QString normalizeServerUrl(const QString &url);
    // An http(s) URL with a host.
    static bool isValidServerUrl(const QString &url);
    static NavidromeConfig load(QSettings &settings);
    void save(QSettings &settings) const;
};
