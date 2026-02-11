#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class LoginPage; }
QT_END_NAMESPACE

class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(QWidget* parent = nullptr);
    ~LoginPage();
    void setBusy(bool busy);

signals:
    void requestForgot();
    void requestLoginSubmit(const QString& email, const QString& password);

private:
    Ui::LoginPage* ui;
};
