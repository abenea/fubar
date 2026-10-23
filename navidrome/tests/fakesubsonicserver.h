#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QTcpServer>
#include <functional>

class QTcpSocket;

// Minimal HTTP/1.1 server on 127.0.0.1 that answers Subsonic REST requests with scripted
// responses and records every request.
class FakeSubsonicServer : public QObject {
    Q_OBJECT
public:
    struct Request {
        QString path;
        QString method;                       // last path component, e.g. "ping"
        QList<QPair<QString, QString>> query; // decoded like a real server ('+' is a space)
        QString rawQuery;
        QHash<QByteArray, QByteArray> headers; // lower-case names

        bool has(const QString &key) const;
        QString value(const QString &key) const;
    };
    struct Response {
        int status = 200;
        QByteArray contentType = "application/json";
        QByteArray body;
        QList<QPair<QByteArray, QByteArray>> headers; // extra response headers
        bool hold = false; // answer only on release(), keep the connection open until then
    };
    using Handler = std::function<Response(const Request &request)>;

    explicit FakeSubsonicServer(QObject *parent = nullptr);
    ~FakeSubsonicServer() override;

    bool listen();
    void close();
    QString url() const;

    // The handler, if set, answers every request; otherwise the response set for the method,
    // otherwise HTTP 404.
    void setResponse(const QString &method, const Response &response);
    void setHandler(Handler handler);
    // Answers every held request with `response`, writing it to the OS right away.
    void release(const Response &response);
    const QList<Request> &requests() const { return requests_; }
    int openConnections() const;

    static Response ok(const QJsonObject &payload = QJsonObject());
    static Response failed(int code, const QString &message);
    static Response http(int status, const QByteArray &body = QByteArray());
    static Response redirect(const QString &location); // 302 Found
    static Response held();

signals:
    void requestReceived();

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket *socket);
    Response responseFor(const Request &request) const;
    static void writeResponse(QTcpSocket *socket, const Response &response);

    QTcpServer server_;
    QHash<QString, Response> responses_;
    Handler handler_;
    QList<Request> requests_;
    QList<QPointer<QTcpSocket>> sockets_;
    QList<QPointer<QTcpSocket>> held_;
};
