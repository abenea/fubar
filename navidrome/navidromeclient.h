#pragma once

#include "navidrome/navidromeconfig.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

class NavidromeClient : public QObject {
    Q_OBJECT
public:
    using Params = QList<QPair<QString, QString>>;
    // `response` is the "subsonic-response" object. `error` is empty on success, otherwise a
    // human readable message without URLs or credentials. Always called asynchronously, and not
    // at all if `context` (nullptr = the client) is destroyed first; the request is then aborted.
    using Callback = std::function<void(const QJsonObject &response, const QString &error)>;

    explicit NavidromeClient(QObject *parent = nullptr);

    // Emits accountChanged() if the server or the username changes, then connectionChanged()
    // unless the connection stays the same.
    void setConfig(const NavidromeConfig &config);
    const NavidromeConfig &config() const { return config_; }
    bool isConfigured() const { return config_.isComplete(); }

    void get(const QString &method, const Params &params, QObject *context, Callback callback);
    void get(const NavidromeConfig &config,
             const QString &method,
             const Params &params,
             QObject *context,
             Callback callback);

    QUrl streamUrl(const QString &songId) const;
    static QUrl apiUrl(const NavidromeConfig &config, const QString &method, const Params &params);

    void getAlbum(const QString &id, QObject *context, Callback callback);
    // `timeMs` is only sent for submissions.
    void scrobble(const QString &songId,
                  bool submission,
                  qint64 timeMs,
                  QObject *context,
                  Callback callback);

signals:
    void accountChanged();
    void connectionChanged();

private:
    static QJsonObject parseReply(QNetworkReply *reply, QString *error);

    NavidromeConfig config_;
    QNetworkAccessManager *network_;
};
