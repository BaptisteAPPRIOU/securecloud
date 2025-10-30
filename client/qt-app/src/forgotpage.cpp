#include "forgotpage.h"
#include "ui_forgotpage.h"

ForgotPage::ForgotPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ForgotPage)
{
    ui->setupUi(this);
}

ForgotPage::~ForgotPage()
{
    delete ui;
}
