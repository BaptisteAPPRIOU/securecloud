#include "AuthClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

AuthClient::AuthClient(const QUrl& baseUrl, QObject* parent)
    : QObject(parent),
      network_(new QNetworkAccessManager(this)),
      base_url_(baseUrl) {}

void AuthClient::setBaseUrl(const QUrl& baseUrl) {
    base_url_ = baseUrl;
}

QUrl AuthClient::baseUrl() const {
    return base_url_;
}

void AuthClient::login(const QString& email, const QString& password) {
    emit loginStarted();

    QNetworkRequest req(buildUrl("/auth/login"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const QJsonObject payload{
        {"email", email.trimmed()},
        {"username", email.trimmed()},
        {"password", password}
    };

    QNetworkReply* reply = network_->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleLoginReply(reply);
        reply->deleteLater();
    });
}

void AuthClient::clearSession() {
    access_token_.clear();
    refresh_token_.clear();
    token_type_ = "Bearer";
    access_exp_epoch_s_ = 0;
    refresh_exp_epoch_s_ = 0;
    mfa_required_ = false;
}

bool AuthClient::hasValidSession() const {
    if (access_token_.isEmpty()) {
        return false;
    }
    if (access_exp_epoch_s_ <= 0) {
        return true;
    }
    return QDateTime::currentSecsSinceEpoch() < access_exp_epoch_s_;
}

QString AuthClient::accessToken() const {
    return access_token_;
}

QString AuthClient::refreshToken() const {
    return refresh_token_;
}

bool AuthClient::mfaRequired() const {
    return mfa_required_;
}

QUrl AuthClient::buildUrl(const QString& path) const {
    QUrl base = base_url_;
    QString basePath = base.path();
    if (basePath.isEmpty()) {
        basePath = "/";
    } else if (!basePath.endsWith('/')) {
        basePath += '/';
    }
    base.setPath(basePath);

    const QString relative = path.startsWith('/') ? path.mid(1) : path;
    return base.resolved(QUrl(relative));
}

QString AuthClient::extractErrorMessage(QNetworkReply* reply) const {
    const QByteArray body = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QString message = obj.value("message").toString();
        if (!message.isEmpty()) {
            return message;
        }
        const QString error = obj.value("error").toString();
        if (!error.isEmpty()) {
            return error;
        }
    }

    if (!reply->errorString().isEmpty()) {
        return reply->errorString();
    }
    return QString("HTTP %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
}

void AuthClient::handleLoginReply(QNetworkReply* reply) {
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
        clearSession();
        emit loginFailed(extractErrorMessage(reply));
        return;
    }

    const QByteArray body = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        clearSession();
        emit loginFailed("Invalid JSON response from auth service");
        return;
    }

    const QJsonObject obj = doc.object();
    const QString access = obj.value("access_token").toString();
    if (access.isEmpty()) {
        clearSession();
        emit loginFailed("Missing access_token in auth response");
        return;
    }

    access_token_ = access;
    refresh_token_ = obj.value("refresh_token").toString();
    token_type_ = obj.value("token_type").toString("Bearer");
    mfa_required_ = obj.value("mfa_required").toBool(false);

    const qint64 nowS = QDateTime::currentSecsSinceEpoch();
    access_exp_epoch_s_ = obj.value("access_exp").toVariant().toLongLong();
    if (access_exp_epoch_s_ <= 0 && obj.contains("expires_in")) {
        access_exp_epoch_s_ = nowS + obj.value("expires_in").toVariant().toLongLong();
    }
    refresh_exp_epoch_s_ = obj.value("refresh_exp").toVariant().toLongLong();

    emit loginSucceeded();
}
