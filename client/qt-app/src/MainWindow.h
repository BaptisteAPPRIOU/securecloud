// MainWindow.h
#pragma once
#include <QMainWindow>
#include <QString>
#include <QUrl>
class QStackedWidget;
class ThemeManager;
class LandingPage; class LoginPage; class ForgotPage;
class AuthClient;
class RealtimeClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ThemeManager* themeMgr, QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void toLanding();
    void toLogin();
    void toForgot();
    void onLoginSubmit(const QString& email, const QString& password);
    void onLoginSucceeded();
    void onLoginFailed(const QString& message);

    void setLightTheme();
    void setDarkTheme();
    void toggleSunlight(bool on);

private:
    ThemeManager* m_theme = nullptr;
    QStackedWidget* m_stack = nullptr;
    LandingPage* m_landing = nullptr;
    LoginPage*   m_login   = nullptr;
    ForgotPage*  m_forgot  = nullptr;
    AuthClient* m_auth = nullptr;
    RealtimeClient* m_realtime = nullptr;
    QUrl m_ws_url;
    bool m_login_in_flight = false;
};
