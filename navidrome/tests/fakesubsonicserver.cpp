#include "navidrome/tests/fakesubsonicserver.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QUrl>
#include <algorithm>

namespace {

QString decode(QByteArray text) { return QUrl::fromPercentEncoding(text.replace('+', ' ')); }

QByteArray reasonPhrase(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 206:
        return "Partial Content";
    case 302:
        return "Found";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 500:
        return "Internal Server Error";
    default:
        return "Unknown";
    }
}

QJsonObject subsonicResponse(const QString &status) {
    return QJsonObject{{"status", status},
                       {"version", "1.16.1"},
                       {"type", "fake"},
                       {"serverVersion", "0.0.1"},
                       {"openSubsonic", true}};
}

FakeSubsonicServer::Response jsonResponse(const QJsonObject &response) {
    FakeSubsonicServer::Response result;
    result.body =
        QJsonDocument(QJsonObject{{"subsonic-response", response}}).toJson(QJsonDocument::Compact);
    return result;
}

} // namespace

bool FakeSubsonicServer::Request::has(const QString &key) const {
    return std::any_of(query.begin(), query.end(), [&](const auto &item) {
        return item.first == key;
    });
}

QString FakeSubsonicServer::Request::value(const QString &key) const {
    for (const auto &item : query) {
        if (item.first == key)
            return item.second;
    }
    return QString();
}

FakeSubsonicServer::FakeSubsonicServer(QObject *parent) : QObject(parent) {
    connect(&server_, &QTcpServer::newConnection, this, &FakeSubsonicServer::onNewConnection);
}

FakeSubsonicServer::~FakeSubsonicServer() { close(); }

bool FakeSubsonicServer::listen() { return server_.listen(QHostAddress::LocalHost); }

void FakeSubsonicServer::close() {
    server_.close();
    for (const QPointer<QTcpSocket> &socket : sockets_) {
        if (socket) {
            socket->abort();
            delete socket;
        }
    }
    sockets_.clear();
    held_.clear();
}

QString FakeSubsonicServer::url() const {
    return QString("http://127.0.0.1:%1").arg(server_.serverPort());
}

void FakeSubsonicServer::setResponse(const QString &method, const Response &response) {
    responses_.insert(method, response);
}

void FakeSubsonicServer::setHandler(Handler handler) { handler_ = std::move(handler); }

void FakeSubsonicServer::release(const Response &response) {
    for (const QPointer<QTcpSocket> &socket : held_) {
        if (socket)
            writeResponse(socket, response);
    }
    held_.clear();
}

int FakeSubsonicServer::openConnections() const {
    return std::count_if(sockets_.begin(), sockets_.end(), [](const QPointer<QTcpSocket> &socket) {
        return socket && socket->state() == QAbstractSocket::ConnectedState;
    });
}

FakeSubsonicServer::Response FakeSubsonicServer::ok(const QJsonObject &payload) {
    QJsonObject response = subsonicResponse("ok");
    for (auto it = payload.begin(); it != payload.end(); ++it)
        response.insert(it.key(), it.value());
    return jsonResponse(response);
}

FakeSubsonicServer::Response FakeSubsonicServer::failed(int code, const QString &message) {
    QJsonObject response = subsonicResponse("failed");
    response.insert("error", QJsonObject{{"code", code}, {"message", message}});
    return jsonResponse(response);
}

FakeSubsonicServer::Response FakeSubsonicServer::http(int status, const QByteArray &body) {
    Response response;
    response.status = status;
    response.contentType = "text/plain";
    response.body = body;
    return response;
}

FakeSubsonicServer::Response FakeSubsonicServer::redirect(const QString &location) {
    Response response = http(302);
    response.headers.append({"Location", location.toUtf8()});
    return response;
}

FakeSubsonicServer::Response FakeSubsonicServer::held() {
    Response response;
    response.hold = true;
    return response;
}

void FakeSubsonicServer::onNewConnection() {
    while (QTcpSocket *socket = server_.nextPendingConnection()) {
        sockets_.append(socket);
        socket->setProperty("buffer", QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void FakeSubsonicServer::onReadyRead(QTcpSocket *socket) {
    QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
    int end = buffer.indexOf("\r\n\r\n");
    if (end < 0) {
        socket->setProperty("buffer", buffer);
        return;
    }
    // One request per connection: every response says "Connection: close".
    disconnect(socket, &QTcpSocket::readyRead, this, nullptr);

    QList<QByteArray> lines = buffer.left(end).split('\n');
    QList<QByteArray> requestLine = lines.takeFirst().trimmed().split(' ');
    QByteArray target = requestLine.value(1);
    QByteArray path = target;
    QByteArray query;
    int queryStart = target.indexOf('?');
    if (queryStart >= 0) {
        path = target.left(queryStart);
        query = target.mid(queryStart + 1);
    }
    Request request;
    request.path = QUrl::fromPercentEncoding(path);
    request.method = request.path.section('/', -1);
    request.rawQuery = QString::fromLatin1(query);
    for (const QByteArray &item : query.split('&')) {
        if (item.isEmpty())
            continue;
        int equals = item.indexOf('=');
        request.query.append(
            {decode(item.left(equals)), equals < 0 ? QString() : decode(item.mid(equals + 1))});
    }
    for (const QByteArray &line : lines) {
        int colon = line.indexOf(':');
        if (colon > 0) {
            request.headers.insert(line.left(colon).trimmed().toLower(),
                                   line.mid(colon + 1).trimmed());
        }
    }
    requests_.append(request);
    emit requestReceived();

    Response response = responseFor(request);
    if (response.hold)
        held_.append(socket);
    else
        writeResponse(socket, response);
}

void FakeSubsonicServer::writeResponse(QTcpSocket *socket, const Response &response) {
    QByteArray head = "HTTP/1.1 " + QByteArray::number(response.status) + " " +
                      reasonPhrase(response.status) + "\r\nContent-Type: " + response.contentType +
                      "\r\nContent-Length: " + QByteArray::number(response.body.size()) +
                      "\r\nConnection: close\r\n";
    for (const auto &header : response.headers)
        head += header.first + ": " + header.second + "\r\n";
    head += "\r\n";
    socket->write(head + response.body);
    socket->flush();
    socket->disconnectFromHost();
}

FakeSubsonicServer::Response FakeSubsonicServer::responseFor(const Request &request) const {
    if (handler_)
        return handler_(request);
    return responses_.value(request.method, http(404, "not found"));
}
