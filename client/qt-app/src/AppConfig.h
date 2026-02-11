#pragma once

#include <QUrl>

class AppConfig {
public:
    static QUrl authBaseUrl();
    static QUrl websocketUrl();
};
