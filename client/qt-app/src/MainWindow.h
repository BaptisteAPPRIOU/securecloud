// MainWindow.h
#pragma once
#include <QMainWindow>
class QStackedWidget;
class ThemeManager;
class LandingPage; class LoginPage; class ForgotPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ThemeManager* themeMgr, QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void toLanding();
    void toLogin();
    void toForgot();

    void setLightTheme();
    void setDarkTheme();
    void toggleSunlight(bool on);

private:
    ThemeManager* m_theme = nullptr;
    QStackedWidget* m_stack = nullptr;
    LandingPage* m_landing = nullptr;
    LoginPage*   m_login   = nullptr;
    ForgotPage*  m_forgot  = nullptr;
};
