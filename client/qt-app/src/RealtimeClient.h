#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QJsonObject;
class QWebSocket;

class RealtimeClient : public QObject {
    Q_OBJECT
public:
    explicit RealtimeClient(QObject* parent = nullptr);

    void connectToServer(const QUrl& url, const QString& accessToken = QString());
    void disconnectFromServer();
    bool isConnected() const;

    void sendTextMessage(const QString& message);
    void sendJsonMessage(const QJsonObject& message);

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString& message);
    void errorOccurred(const QString& message);

private:
    QWebSocket* socket_;
    QUrl current_url_;
};
