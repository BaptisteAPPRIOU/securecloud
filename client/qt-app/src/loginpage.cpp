#include "loginpage.h"
#include "ui_loginpage.h"
#include <QPushButton>
#include <QLineEdit>

LoginPage::LoginPage(QWidget* parent)
    : QWidget(parent), ui(new Ui::LoginPage)
{
    ui->setupUi(this);

    // Nav vers Forgot
    connect(ui->linkForgot, &QPushButton::clicked, this, &LoginPage::requestForgot);

    // Soumission Login
    connect(ui->btnLogin, &QPushButton::clicked, this, [this]{
        emit requestLoginSubmit(ui->editEmail->text().trimmed(),
                                ui->editPwd->text());
    });
}

LoginPage::~LoginPage() { delete ui; }

void LoginPage::setBusy(bool busy) {
    ui->btnLogin->setEnabled(!busy);
    ui->linkForgot->setEnabled(!busy);
    ui->editEmail->setEnabled(!busy);
    ui->editPwd->setEnabled(!busy);
    ui->btnLogin->setText(busy ? "Connexion..." : "Connexion");
}
