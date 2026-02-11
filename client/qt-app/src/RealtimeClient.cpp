#include "RealtimeClient.h"

#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QWebSocket>

RealtimeClient::RealtimeClient(QObject* parent)
    : QObject(parent),
      socket_(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this)) {
    connect(socket_, &QWebSocket::connected, this, &RealtimeClient::connected);
    connect(socket_, &QWebSocket::disconnected, this, &RealtimeClient::disconnected);
    connect(socket_, &QWebSocket::textMessageReceived, this, &RealtimeClient::messageReceived);

    connect(socket_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorOccurred(socket_->errorString());
    });
}

void RealtimeClient::connectToServer(const QUrl& url, const QString& accessToken) {
    if (!url.isValid()) {
        emit errorOccurred("Invalid WebSocket URL");
        return;
    }

    current_url_ = url;
    QNetworkRequest req(current_url_);
    if (!accessToken.isEmpty()) {
        req.setRawHeader("Authorization", QByteArray("Bearer ") + accessToken.toUtf8());
    }

    socket_->open(req);
}

void RealtimeClient::disconnectFromServer() {
    socket_->close();
}

bool RealtimeClient::isConnected() const {
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void RealtimeClient::sendTextMessage(const QString& message) {
    if (isConnected()) {
        socket_->sendTextMessage(message);
    }
}

void RealtimeClient::sendJsonMessage(const QJsonObject& message) {
    sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}
