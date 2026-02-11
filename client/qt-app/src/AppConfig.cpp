#include "AppConfig.h"

#include <QString>
#include <QtGlobal>

namespace {

QUrl urlFromEnv(const char* key, const char* fallback) {
    const QString envValue = qEnvironmentVariable(key);
    if (!envValue.isEmpty()) {
        return QUrl::fromUserInput(envValue);
    }
    return QUrl::fromUserInput(QString::fromUtf8(fallback));
}

} // namespace

QUrl AppConfig::authBaseUrl() {
    return urlFromEnv("AUTH_SERVICE_URL", "http://127.0.0.1:8001");
}

QUrl AppConfig::websocketUrl() {
    const QString explicitWsUrl = qEnvironmentVariable("SECURECLOUD_WS_URL");
    if (!explicitWsUrl.isEmpty()) {
        return QUrl::fromUserInput(explicitWsUrl);
    }

    const QString wsPort = qEnvironmentVariable("WEBSOCKET_PORT");
    if (!wsPort.isEmpty()) {
        return QUrl::fromUserInput(QString("ws://127.0.0.1:%1/ws").arg(wsPort));
    }

    return QUrl::fromUserInput("ws://127.0.0.1:8005/ws");
}
