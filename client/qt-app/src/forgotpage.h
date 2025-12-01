#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class ForgotPage; }
QT_END_NAMESPACE

class ForgotPage : public QWidget {
    Q_OBJECT
public:
    explicit ForgotPage(QWidget* parent = nullptr);
    ~ForgotPage();

signals:
    void requestSendReset(const QString& email);
    void requestBackToLogin();

private:
    Ui::ForgotPage* ui;
};
