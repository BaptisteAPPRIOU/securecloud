#include "landingpage.h"
#include "ui_landingpage.h"
#include <QPushButton>

LandingPage::LandingPage(QWidget* parent)
    : QWidget(parent), ui(new Ui::LandingPage)
{
    ui->setupUi(this);

    // Navigation signals
    connect(ui->btnGoLogin,  &QPushButton::clicked, this, &LandingPage::requestLogin);
    connect(ui->btnGoForgot, &QPushButton::clicked, this, &LandingPage::requestForgot);
}

LandingPage::~LandingPage() { delete ui; }
