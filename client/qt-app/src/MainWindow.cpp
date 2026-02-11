// MainWindow.cpp
#include "MainWindow.h"

#include "AppConfig.h"
#include "AuthClient.h"
#include "RealtimeClient.h"
#include "ThemeManager.h"
#include "forgotpage.h"
#include "landingpage.h"
#include "loginpage.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>

MainWindow::MainWindow(ThemeManager* themeMgr, QWidget* parent)
    : QMainWindow(parent), m_theme(themeMgr) {
    auto view = menuBar()->addMenu("View");
    auto actLight = view->addAction("Theme Light (D)");
    actLight->setCheckable(true);
    actLight->setChecked(true);
    auto actDark = view->addAction("Theme Dark (C)");
    actDark->setCheckable(true);
    auto actSun = view->addAction("Plein soleil");
    actSun->setCheckable(true);
    connect(actLight, &QAction::triggered, this, &MainWindow::setLightTheme);
    connect(actDark, &QAction::triggered, this, &MainWindow::setDarkTheme);
    connect(actSun, &QAction::toggled, this, &MainWindow::toggleSunlight);

    auto nav = menuBar()->addMenu("Navigate");
    auto goLanding = nav->addAction("Accueil");
    auto goLogin = nav->addAction("Connexion");
    auto goForgot = nav->addAction("Mot de passe oublie");
    connect(goLanding, &QAction::triggered, this, &MainWindow::toLanding);
    connect(goLogin, &QAction::triggered, this, &MainWindow::toLogin);
    connect(goForgot, &QAction::triggered, this, &MainWindow::toForgot);

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_landing = new LandingPage(this);
    m_login = new LoginPage(this);
    m_forgot = new ForgotPage(this);

    m_auth = new AuthClient(AppConfig::authBaseUrl(), this);
    m_realtime = new RealtimeClient(this);
    m_ws_url = AppConfig::websocketUrl();

    m_stack->addWidget(m_landing); // 0
    m_stack->addWidget(m_login);   // 1
    m_stack->addWidget(m_forgot);  // 2

    connect(m_landing, &LandingPage::requestLogin, this, &MainWindow::toLogin);
    connect(m_landing, &LandingPage::requestForgot, this, &MainWindow::toForgot);

    connect(m_login, &LoginPage::requestForgot, this, &MainWindow::toForgot);
    connect(m_login, &LoginPage::requestLoginSubmit, this, &MainWindow::onLoginSubmit);

    connect(m_auth, &AuthClient::loginSucceeded, this, &MainWindow::onLoginSucceeded);
    connect(m_auth, &AuthClient::loginFailed, this, &MainWindow::onLoginFailed);

    connect(m_realtime, &RealtimeClient::connected, this, []() {
        qInfo() << "[WebSocket] connected";
    });
    connect(m_realtime, &RealtimeClient::disconnected, this, []() {
        qInfo() << "[WebSocket] disconnected";
    });
    connect(m_realtime, &RealtimeClient::errorOccurred, this, [](const QString& message) {
        qWarning() << "[WebSocket] error:" << message;
    });

    connect(m_forgot, &ForgotPage::requestSendReset, this, [](const QString& email) {
        qInfo() << "[Forgot] send reset to" << email;
    });
    connect(m_forgot, &ForgotPage::requestBackToLogin, this, &MainWindow::toLogin);

    toLanding();
}

MainWindow::~MainWindow() = default;

void MainWindow::toLanding() { m_stack->setCurrentIndex(0); }
void MainWindow::toLogin() { m_stack->setCurrentIndex(1); }
void MainWindow::toForgot() { m_stack->setCurrentIndex(2); }

void MainWindow::onLoginSubmit(const QString& email, const QString& password) {
    if (m_login_in_flight) {
        return;
    }

    if (email.trimmed().isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Connexion", "Email and password are required.");
        return;
    }

    m_login_in_flight = true;
    m_auth->login(email, password);
}

void MainWindow::onLoginSucceeded() {
    m_login_in_flight = false;

    QString message = "Authentication succeeded.";
    if (m_auth->mfaRequired()) {
        message += " MFA is required for this account.";
    }
    QMessageBox::information(this, "Connexion", message);

    toLanding();

    if (m_ws_url.isValid()) {
        m_realtime->connectToServer(m_ws_url, m_auth->accessToken());
    } else {
        qWarning() << "[WebSocket] invalid URL";
    }
}

void MainWindow::onLoginFailed(const QString& message) {
    m_login_in_flight = false;
    QMessageBox::warning(this, "Connexion failed", message);
}

void MainWindow::setLightTheme() { m_theme->applyLightD(*qApp, false); }
void MainWindow::setDarkTheme() { m_theme->applyDarkC(*qApp, false); }
void MainWindow::toggleSunlight(bool on) { m_theme->setSunlight(*qApp, on); }
