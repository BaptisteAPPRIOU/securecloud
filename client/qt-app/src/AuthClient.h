#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

class AuthClient : public QObject {
    Q_OBJECT
public:
    explicit AuthClient(const QUrl& baseUrl, QObject* parent = nullptr);

    void setBaseUrl(const QUrl& baseUrl);
    QUrl baseUrl() const;

    void login(const QString& email, const QString& password);
    void clearSession();

    bool hasValidSession() const;
    QString accessToken() const;
    QString refreshToken() const;
    bool mfaRequired() const;

signals:
    void loginStarted();
    void loginSucceeded();
    void loginFailed(const QString& message);

private:
    QUrl buildUrl(const QString& path) const;
    void handleLoginReply(QNetworkReply* reply);
    QString extractErrorMessage(QNetworkReply* reply) const;

    QNetworkAccessManager* network_;
    QUrl base_url_;
    QString access_token_;
    QString refresh_token_;
    QString token_type_{"Bearer"};
    qint64 access_exp_epoch_s_{0};
    qint64 refresh_exp_epoch_s_{0};
    bool mfa_required_{false};
};
