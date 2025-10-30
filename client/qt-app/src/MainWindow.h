#pragma once
#include <QMainWindow>
#include <QPoint>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ThemeManager;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(ThemeManager *themeMgr, QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void toLanding();
  void toLogin();
  void toForgot();

  void setLightTheme();
  void setDarkTheme();
  void toggleSunlight(bool on);

  // Auto-connected slot for the logo widget's customContextMenuRequested
  // (named in the .ui as "logo" with the signal customContextMenuRequested(QPoint)).
  void on_logo_customContextMenuRequested(const QPoint &pos);

private:
  Ui::MainWindow *ui;
  ThemeManager *m_theme;
};