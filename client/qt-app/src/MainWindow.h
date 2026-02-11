// MainWindow.h
#pragma once
#include <QMainWindow>
#include <QDateTime>
class QStackedWidget;
class ThemeManager;
class QNetworkAccessManager;
class QNetworkRequest;
class LandingPage; class LoginPage; class ForgotPage; class HomePage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ThemeManager* themeMgr, QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void toLanding();
    void toLogin();
    void toForgot();
    void toHome();
    void performLogout();

    void submitLogin(const QString& email, const QString& password);

    void setLightTheme();
    void setDarkTheme();
    void toggleSunlight(bool on);

private:
    void applyAuthHeader(QNetworkRequest& req) const;

    ThemeManager* m_theme = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    bool m_loginInFlight = false;
    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_accessTokenExpiresAt;
    QStackedWidget* m_stack = nullptr;
    LandingPage* m_landing = nullptr;
    LoginPage*   m_login   = nullptr;
    ForgotPage*  m_forgot  = nullptr;
    HomePage*    m_home    = nullptr;
};
