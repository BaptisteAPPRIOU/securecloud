#include "forgotpage.h"
#include "ui_ForgotPage.h"
#include <QPushButton>
#include <QLineEdit>

ForgotPage::ForgotPage(QWidget* parent)
    : QWidget(parent), ui(new Ui::ForgotPage)
{
    ui->setupUi(this);

    connect(ui->btnReceive, &QPushButton::clicked, this, [this]{
        emit requestSendReset(ui->editResetEmail->text().trimmed());
    });

    // Option: double-clic sur le titre pour revenir (ou ajoute un bouton Retour si tu veux)
    connect(ui->forgotTitle, &QLabel::linkActivated, this, [this]{ emit requestBackToLogin(); });
}

ForgotPage::~ForgotPage() { delete ui; }
