#include "navidrome/navidromeclient.h"

#include <QDebug>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>
#include <memory>

namespace {

constexpr const char *kApiVersion = "1.16.1";
constexpr const char *kClientName = "fubar";
constexpr int kTransferTimeoutMs = 30000;

QUrl buildUrl(const NavidromeConfig &config,
              const QString &method,
              const NavidromeClient::Params &params,
              bool json) {
    NavidromeClient::Params query{{"u", config.username},
                                  {"t", config.token},
                                  {"s", config.salt},
                                  {"v", kApiVersion},
                                  {"c", kClientName}};
    if (json)
        query.append(qMakePair(QString("f"), QString("json")));
    query += params;
    QStringList items;
    for (const auto &item : query)
        items << QString::fromLatin1(QUrl::toPercentEncoding(item.first) + '=' +
                                     QUrl::toPercentEncoding(item.second));
    QUrl url(config.serverUrl + "/rest/" + method);
    url.setQuery(items.join('&'), QUrl::StrictMode);
    return url;
}

QString networkErrorMessage(QNetworkReply::NetworkError error) {
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
        return "Connection refused";
    case QNetworkReply::HostNotFoundError:
        return "Host not found";
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        return "Timed out";
    case QNetworkReply::SslHandshakeFailedError:
        return "SSL error";
    // Only redirects to the same origin are followed
    case QNetworkReply::InsecureRedirectError:
        return "The server redirected to another address; enter that address (e.g. https://…)";
    default:
        return QString("Network error (%1)").arg(int(error));
    }
}

} // namespace

NavidromeClient::NavidromeClient(QObject *parent)
    : QObject(parent), network_(new QNetworkAccessManager(this)) {}

void NavidromeClient::setConfig(const NavidromeConfig &config) {
    bool sameAccount = config_.sameAccount(config);
    bool sameConnection = config_.sameConnection(config);
    config_ = config;
    if (!sameAccount)
        emit accountChanged();
    if (!sameConnection)
        emit connectionChanged();
}

void NavidromeClient::get(const QString &method,
                          const Params &params,
                          QObject *context,
                          Callback callback) {
    get(config_, method, params, context, std::move(callback));
}

void NavidromeClient::get(const NavidromeConfig &config,
                          const QString &method,
                          const Params &params,
                          QObject *context,
                          Callback callback) {
    if (!context)
        context = this;
    if (!config.isComplete()) {
        QTimer::singleShot(0, context, [method, callback]() {
            QString error = "Navidrome is not configured";
            qDebug() << "[Navidrome]" << method << error;
            callback(QJsonObject(), error);
        });
        return;
    }

    QNetworkRequest request(apiUrl(config, method, params));
    request.setTransferTimeout(kTransferTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString(kClientName));
    QNetworkReply *reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

    auto finished = std::make_shared<QMetaObject::Connection>();
    auto destroyed = std::make_shared<QMetaObject::Connection>();
    *finished = connect(reply, &QNetworkReply::finished, context, [=]() {
        QObject::disconnect(*destroyed);
        QString error;
        QJsonObject response = parseReply(reply, &error);
        if (!error.isEmpty())
            qDebug() << "[Navidrome]" << method << error;
        callback(response, error);
    });
    if (context != this) {
        *destroyed = connect(context, &QObject::destroyed, reply, [=]() {
            QObject::disconnect(*finished);
            reply->abort();
        });
    }
}

QJsonObject NavidromeClient::parseReply(QNetworkReply *reply, QString *error) {
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 400) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        *error = QString("HTTP %1 %2").arg(status).arg(reason).trimmed();
        return QJsonObject();
    }
    if (reply->error() != QNetworkReply::NoError) {
        *error = networkErrorMessage(reply->error());
        return QJsonObject();
    }
    QJsonObject response =
        QJsonDocument::fromJson(reply->readAll()).object().value("subsonic-response").toObject();
    QString responseStatus = response.value("status").toString();
    if (responseStatus == "ok")
        return response;
    if (responseStatus == "failed") {
        QJsonObject failure = response.value("error").toObject();
        *error = failure.value("message").toString();
        if (error->isEmpty())
            *error = QString("Error %1").arg(failure.value("code").toInt());
        return response;
    }
    *error = "Invalid response from server";
    return QJsonObject();
}

QUrl NavidromeClient::streamUrl(const QString &songId) const {
    return buildUrl(config_, "stream", {{"id", songId}}, false);
}

QUrl NavidromeClient::apiUrl(const NavidromeConfig &config,
                             const QString &method,
                             const Params &params) {
    return buildUrl(config, method, params, true);
}

void NavidromeClient::getAlbum(const QString &id, QObject *context, Callback callback) {
    get("getAlbum", {{"id", id}}, context, std::move(callback));
}

void NavidromeClient::scrobble(const QString &songId,
                               bool submission,
                               qint64 timeMs,
                               QObject *context,
                               Callback callback) {
    Params params{{"id", songId}, {"submission", submission ? "true" : "false"}};
    if (submission)
        params.append(qMakePair(QString("time"), QString::number(timeMs)));
    get("scrobble", params, context, std::move(callback));
}
