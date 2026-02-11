// MainWindow.cpp
#include "MainWindow.h"
#include "ThemeManager.h"
#include "landingpage.h"
#include "loginpage.h"
#include "forgotpage.h"
#include "homepage.h"
#include <QStackedWidget>
#include <QMenuBar>
#include <QAction>
#include <QApplication>
#include <QtGlobal>
#include <QByteArray>
#include <QUrl>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QStatusBar>
#include <QVBoxLayout>

namespace {
QUrl apiBaseUrl() {
    QByteArray env = qgetenv("SECURECLOUD_API_BASE");
    if (!env.isEmpty()) {
        QUrl url = QUrl(QString::fromUtf8(env));
        if (url.isValid()) {
            return url;
        }
    }
    return QUrl(QStringLiteral("http://localhost:8443"));
}

constexpr int kPageLanding = 0;
constexpr int kPageLogin = 1;
constexpr int kPageForgot = 2;
constexpr int kPageHome = 3;

QString decodeJwtClaims(const QString& jwtToken) {
    const QStringList parts = jwtToken.split('.');
    if (parts.size() < 2) {
        return QStringLiteral("Unable to decode claims: invalid JWT format.");
    }

    QByteArray payload = parts.at(1).toUtf8();
    payload.replace('-', '+');
    payload.replace('_', '/');
    while (payload.size() % 4 != 0) {
        payload.append('=');
    }

    const QByteArray decoded = QByteArray::fromBase64(payload);
    if (decoded.isEmpty()) {
        return QStringLiteral("Unable to decode claims: empty JWT payload.");
    }

    QJsonParseError parse_error{};
    const QJsonDocument claims_doc = QJsonDocument::fromJson(decoded, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        return QStringLiteral("Unable to parse claims JSON.\n\nRaw payload:\n%1")
            .arg(QString::fromUtf8(decoded));
    }
    if (!claims_doc.isObject()) {
        return QStringLiteral("JWT payload is not a JSON object.\n\nRaw payload:\n%1")
            .arg(QString::fromUtf8(decoded));
    }

    return QString::fromUtf8(claims_doc.toJson(QJsonDocument::Indented));
}

void showAuthPopup(QWidget* parent, const QString& userEmail, const QString& accessToken) {
    QDialog popup(parent);
    popup.setWindowTitle(QStringLiteral("Authentication Successful"));
    popup.setModal(true);
    popup.resize(760, 560);

    auto* layout = new QVBoxLayout(&popup);

    auto* intro = new QLabel(
        QStringLiteral("Authenticated as %1.\nClose this popup to access the home page.")
            .arg(userEmail),
        &popup
    );
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* claimsLabel = new QLabel(QStringLiteral("User claims (decoded from access JWT):"), &popup);
    layout->addWidget(claimsLabel);

    auto* claimsView = new QPlainTextEdit(&popup);
    claimsView->setReadOnly(true);
    claimsView->setMinimumHeight(180);
    claimsView->setPlainText(decodeJwtClaims(accessToken));
    layout->addWidget(claimsView);

    auto* jwtLabel = new QLabel(QStringLiteral("Access JWT:"), &popup);
    layout->addWidget(jwtLabel);

    auto* jwtView = new QPlainTextEdit(&popup);
    jwtView->setReadOnly(true);
    jwtView->setLineWrapMode(QPlainTextEdit::NoWrap);
    jwtView->setPlainText(accessToken.isEmpty() ? QStringLiteral("<empty access token>") : accessToken);
    layout->addWidget(jwtView);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &popup);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &popup, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &popup, &QDialog::accept);
    layout->addWidget(buttons);

    popup.exec();
}
}

MainWindow::MainWindow(ThemeManager* themeMgr, QWidget* parent)
    : QMainWindow(parent), m_theme(themeMgr)
{
    // Menu
    auto view = menuBar()->addMenu("View");
    auto actLight = view->addAction("Theme Light (D)"); actLight->setCheckable(true); actLight->setChecked(true);
    auto actDark  = view->addAction("Theme Dark (C)");  actDark->setCheckable(true);
    auto actSun   = view->addAction("Plein soleil");    actSun->setCheckable(true);
    connect(actLight, &QAction::triggered, this, &MainWindow::setLightTheme);
    connect(actDark,  &QAction::triggered, this, &MainWindow::setDarkTheme);
    connect(actSun,   &QAction::toggled,   this, &MainWindow::toggleSunlight);

    auto nav = menuBar()->addMenu("Navigate");
    auto goLanding = nav->addAction("Accueil");
    auto goLogin   = nav->addAction("Connexion");
    auto goForgot  = nav->addAction("Mot de passe oublié");
    auto goHome    = nav->addAction("Conversations");
    connect(goLanding, &QAction::triggered, this, &MainWindow::toLanding);
    connect(goLogin,   &QAction::triggered, this, &MainWindow::toLogin);
    connect(goForgot,  &QAction::triggered, this, &MainWindow::toForgot);
    connect(goHome,    &QAction::triggered, this, &MainWindow::toHome);

    // Stack + pages
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_landing = new LandingPage(this);
    m_login   = new LoginPage(this);
    m_forgot  = new ForgotPage(this);
    m_home    = new HomePage(this);

    m_network = new QNetworkAccessManager(this);

    m_stack->addWidget(m_landing); // 0
    m_stack->addWidget(m_login);   // 1
    m_stack->addWidget(m_forgot);  // 2
    m_stack->addWidget(m_home);    // 3

    // Connexions inter-pages
    connect(m_landing, &LandingPage::requestLogin,  this, &MainWindow::toLogin);
    connect(m_landing, &LandingPage::requestForgot, this, &MainWindow::toForgot);

    connect(m_login, &LoginPage::requestForgot, this, &MainWindow::toForgot);
    connect(m_login, &LoginPage::requestLoginSubmit, this, &MainWindow::submitLogin);

    connect(m_forgot, &ForgotPage::requestSendReset, this, [](const QString& email){
        qInfo() << "[Forgot] send reset to" << email;
    });
    connect(m_forgot, &ForgotPage::requestBackToLogin, this, &MainWindow::toLogin);
    connect(m_home, &HomePage::requestLogout, this, &MainWindow::performLogout);

    toLanding();
}

MainWindow::~MainWindow() = default;

void MainWindow::applyAuthHeader(QNetworkRequest& req) const {
    if (m_accessToken.isEmpty()) {
        return;
    }
    if (m_accessTokenExpiresAt.isValid() &&
        QDateTime::currentDateTimeUtc() > m_accessTokenExpiresAt) {
        return;
    }
    req.setRawHeader("Authorization", QByteArray("Bearer ") + m_accessToken.toUtf8());
}

void MainWindow::submitLogin(const QString& email, const QString& password) {
    if (m_loginInFlight) {
        return;
    }
    m_loginInFlight = true;
    if (m_login) {
        m_login->setBusy(true);
    }

    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }

    QUrl base = apiBaseUrl();
    QUrl url = base.resolved(QUrl("/api/login"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload.insert("email", email);
    payload.insert("password", password);
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    qInfo() << "Login request:" << url.toString() << "bytes=" << body.size();
    QNetworkReply* reply = m_network->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, email]() {
        auto finish = [this]() {
            m_loginInFlight = false;
            if (m_login) {
                m_login->setBusy(false);
            }
        };
        auto clear_tokens = [this]() {
            m_accessToken.clear();
            m_refreshToken.clear();
            m_accessTokenExpiresAt = QDateTime();
        };

        reply->deleteLater();
        QByteArray data = reply->readAll();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonParseError parse_error{};
        QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
        const QString error_text = reply->errorString();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Login network error:" << error_text
                       << "status=" << status << "bytes=" << data.size();
        } else {
            qInfo() << "Login response:" << "status=" << status << "bytes=" << data.size();
        }
        if (parse_error.error != QJsonParseError::NoError) {
            qWarning() << "Login JSON parse error:" << parse_error.errorString();
        }

        auto extract_message = [&](const QJsonObject& obj) {
            const QString message = obj.value("message").toString();
            if (!message.isEmpty()) {
                return message;
            }
            const QString error = obj.value("error").toString();
            return error;
        };

        const bool http_ok = status >= 200 && status < 300;
        if (!http_ok || reply->error() != QNetworkReply::NoError) {
            QString message = reply->errorString();
            if (doc.isObject()) {
                QString server_msg = extract_message(doc.object());
                if (!server_msg.isEmpty()) {
                    message = server_msg;
                }
            } else if (!data.isEmpty() && parse_error.error != QJsonParseError::NoError) {
                message += QStringLiteral("\n") + QString::fromUtf8(data);
            }
            clear_tokens();
            finish();
            qWarning() << "Login failed:" << message;
            QMessageBox::critical(this, QStringLiteral("Login failed"), message);
            return;
        }

        if (!doc.isObject()) {
            clear_tokens();
            finish();
            QMessageBox::critical(this, QStringLiteral("Login failed"), QStringLiteral("Invalid server response."));
            return;
        }

        QJsonObject obj = doc.object();
        m_accessToken = obj.value("access_token").toString();
        m_refreshToken = obj.value("refresh_token").toString();
        const int expires_in = obj.value("expires_in").toInt(0);
        if (expires_in > 0) {
            m_accessTokenExpiresAt = QDateTime::currentDateTimeUtc().addSecs(expires_in);
        } else {
            m_accessTokenExpiresAt = QDateTime();
        }

        QString user_email = email;
        if (obj.contains("user") && obj.value("user").isObject()) {
            QJsonObject user = obj.value("user").toObject();
            const QString server_email = user.value("email").toString();
            if (!server_email.isEmpty()) {
                user_email = server_email;
            }
        }
        if (m_home) {
            m_home->setCurrentUserEmail(user_email);
        }
        finish();
        qInfo() << "Login success for" << user_email << "status=" << status;
        showAuthPopup(this, user_email, m_accessToken);
        toHome();
        statusBar()->showMessage(QStringLiteral("Login successful for %1").arg(user_email), 5000);
    });
}

void MainWindow::toLanding() { m_stack->setCurrentIndex(kPageLanding); }
void MainWindow::toLogin()   { m_stack->setCurrentIndex(kPageLogin); }
void MainWindow::toForgot()  { m_stack->setCurrentIndex(kPageForgot); }
void MainWindow::toHome()    { m_stack->setCurrentIndex(kPageHome); }

void MainWindow::performLogout() {
    // Keep a copy for best-effort server-side invalidation.
    const QString access_token = m_accessToken;
    const QString refresh_token = m_refreshToken;

    // Local session cleanup.
    m_accessToken.clear();
    m_refreshToken.clear();
    m_accessTokenExpiresAt = QDateTime();

    if (m_home) {
        m_home->setCurrentUserEmail(QString());
    }

    toLanding();
    statusBar()->showMessage(QStringLiteral("Logged out."), 3000);

    if (access_token.isEmpty()) {
        return;
    }

    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }

    QUrl base = apiBaseUrl();
    QUrl url = base.resolved(QUrl("/api/logout"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QByteArray("Bearer ") + access_token.toUtf8());

    QJsonObject payload;
    if (!refresh_token.isEmpty()) {
        payload.insert("refresh_token", refresh_token);
    }
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = m_network->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Logout API call failed:" << reply->errorString() << "status=" << status;
        } else {
            qInfo() << "Logout API call completed with status" << status;
        }
        reply->deleteLater();
    });
}

void MainWindow::setLightTheme() { m_theme->applyLightD(*qApp, false); }
void MainWindow::setDarkTheme()  { m_theme->applyDarkC(*qApp, false); }
void MainWindow::toggleSunlight(bool on) { m_theme->setSunlight(*qApp, on); }
