#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ThemeManager.h"
#include <QApplication>

MainWindow::MainWindow(ThemeManager *themeMgr, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_theme(themeMgr) {
  ui->setupUi(this);

  // Centered widths for landing controls
  ui->btnGoLogin->setMinimumWidth(220);
  ui->btnGoForgot->setMinimumWidth(220);
  ui->btnGoForgot->setProperty("class", "link");

  // Navigation connections respecting wireframes
  connect(ui->btnGoLogin, &QPushButton::clicked, this, &MainWindow::toLogin);
  connect(ui->btnGoForgot, &QPushButton::clicked, this, &MainWindow::toForgot);
  connect(ui->linkForgot, &QPushButton::clicked, this, &MainWindow::toForgot);

  // Menu navigation
  connect(ui->actionGoLanding, &QAction::triggered, this,
          &MainWindow::toLanding);
  connect(ui->actionGoLogin, &QAction::triggered, this, &MainWindow::toLogin);
  connect(ui->actionGoForgot, &QAction::triggered, this, &MainWindow::toForgot);

  // Theme actions
  ui->actionLight->setCheckable(true);
  ui->actionDark->setCheckable(true);

  connect(ui->actionLight, &QAction::triggered, this,
          &MainWindow::setLightTheme);
  connect(ui->actionDark, &QAction::triggered, this, &MainWindow::setDarkTheme);
  connect(ui->actionSunlight, &QAction::toggled, this,
          &MainWindow::toggleSunlight);

  toLanding();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::toLanding() { ui->stack->setCurrentWidget(ui->pageLanding); }
void MainWindow::toLogin() { ui->stack->setCurrentWidget(ui->pageLogin); }
void MainWindow::toForgot() { ui->stack->setCurrentWidget(ui->pageForgot); }

void MainWindow::setLightTheme() {
  ui->actionLight->setChecked(true);
  ui->actionDark->setChecked(false);
  m_theme->applyLightD(*qApp, ui->actionSunlight->isChecked());
}

void MainWindow::setDarkTheme() {
  ui->actionLight->setChecked(false);
  ui->actionDark->setChecked(true);
  m_theme->applyDarkC(*qApp, ui->actionSunlight->isChecked());
}

void MainWindow::toggleSunlight(bool on) { m_theme->setSunlight(*qApp, on); }
void MainWindow::on_logo_customContextMenuRequested(const QPoint &pos)
{

}

