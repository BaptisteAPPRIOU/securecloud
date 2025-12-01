// MainWindow.cpp
#include "MainWindow.h"
#include "ThemeManager.h"
#include "landingpage.h"
#include "loginpage.h"
#include "forgotpage.h"
#include <QStackedWidget>
#include <QMenuBar>
#include <QAction>
#include <QApplication>

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
    connect(goLanding, &QAction::triggered, this, &MainWindow::toLanding);
    connect(goLogin,   &QAction::triggered, this, &MainWindow::toLogin);
    connect(goForgot,  &QAction::triggered, this, &MainWindow::toForgot);

    // Stack + pages
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_landing = new LandingPage(this);
    m_login   = new LoginPage(this);
    m_forgot  = new ForgotPage(this);

    m_stack->addWidget(m_landing); // 0
    m_stack->addWidget(m_login);   // 1
    m_stack->addWidget(m_forgot);  // 2

    // Connexions inter-pages
    connect(m_landing, &LandingPage::requestLogin,  this, &MainWindow::toLogin);
    connect(m_landing, &LandingPage::requestForgot, this, &MainWindow::toForgot);

    connect(m_login, &LoginPage::requestForgot, this, &MainWindow::toForgot);
    connect(m_login, &LoginPage::requestLoginSubmit, this, [](const QString& email, const QString& pwd){
        // TODO: logique d’auth (placeholder)
        qInfo() << "[Login]" << email << pwd;
    });

    connect(m_forgot, &ForgotPage::requestSendReset, this, [](const QString& email){
        qInfo() << "[Forgot] send reset to" << email;
    });
    connect(m_forgot, &ForgotPage::requestBackToLogin, this, &MainWindow::toLogin);

    toLanding();
}

MainWindow::~MainWindow() = default;

void MainWindow::toLanding() { m_stack->setCurrentIndex(0); }
void MainWindow::toLogin()   { m_stack->setCurrentIndex(1); }
void MainWindow::toForgot()  { m_stack->setCurrentIndex(2); }

void MainWindow::setLightTheme() { m_theme->applyLightD(*qApp, false); }
void MainWindow::setDarkTheme()  { m_theme->applyDarkC(*qApp, false); }
void MainWindow::toggleSunlight(bool on) { m_theme->setSunlight(*qApp, on); }
