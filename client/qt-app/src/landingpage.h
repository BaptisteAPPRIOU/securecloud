#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class LandingPage; }
QT_END_NAMESPACE

class LandingPage : public QWidget {
    Q_OBJECT
public:
    explicit LandingPage(QWidget* parent = nullptr);
    ~LandingPage();

signals:
    void requestLogin();
    void requestForgot();

private:
    Ui::LandingPage* ui;
};
