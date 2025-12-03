#include "loginpage.h"
#include "ui_LoginPage.h"
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
