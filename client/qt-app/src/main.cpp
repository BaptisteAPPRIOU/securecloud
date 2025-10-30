#include "MainWindow.h"
#include "ThemeManager.h"
#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  ThemeManager theme;
  theme.applyLightD(app, false); // D par défaut

  MainWindow w(&theme);
  w.resize(720, 480);
  w.show();

  return app.exec();
}