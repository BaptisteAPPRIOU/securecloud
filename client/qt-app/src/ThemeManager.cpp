#include "ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QPalette>

// ----- Palettes "pures" -----

static void applyPaletteLight_D(QApplication &app) {
  QPalette pal;
  pal.setColor(QPalette::Window, QColor("#EDF2F7"));
  pal.setColor(QPalette::Base, QColor("#FFFFFF"));
  pal.setColor(QPalette::Text, QColor("#111111"));
  pal.setColor(QPalette::WindowText, QColor("#111111"));
  pal.setColor(QPalette::Button, QColor("#E2211C"));
  pal.setColor(QPalette::ButtonText, QColor("#FFFFFF"));
  pal.setColor(QPalette::Highlight, QColor("#E2211C"));
  pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
  pal.setColor(QPalette::Link, QColor("#1D4ED8"));
  pal.setColor(QPalette::LinkVisited, QColor("#1D4ED8"));
  app.setPalette(pal);
}

static void applyPaletteDark_C(QApplication &app) {
  QPalette pal;
  pal.setColor(QPalette::Window, QColor("#0B0B0C"));
  pal.setColor(QPalette::Base, QColor("#16181A"));
  pal.setColor(QPalette::Text, QColor("#FFFFFF"));
  pal.setColor(QPalette::WindowText, QColor("#FFFFFF"));
  pal.setColor(QPalette::Button, QColor("#FF2B1C"));
  pal.setColor(QPalette::ButtonText, QColor("#FFFFFF"));
  pal.setColor(QPalette::Highlight, QColor("#FF2B1C"));
  pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
  pal.setColor(QPalette::Link, QColor("#64B5F6"));
  pal.setColor(QPalette::LinkVisited, QColor("#64B5F6"));
  app.setPalette(pal);
}

// ----- Chargement QSS -----

void ThemeManager::applyQss(QApplication &app, const QString &baseQss,
                            bool sunlight) {
  QFile f(baseQss);
  if (f.open(QIODevice::ReadOnly)) {
    QString qss = QString::fromUtf8(f.readAll());
    f.close();

    if (sunlight) {
      QFile s(":/qss/sunlight_overlay.qss");
      if (s.open(QIODevice::ReadOnly)) {
        qss += "\n";
        qss += QString::fromUtf8(s.readAll());
        s.close();
      }
    }
    app.setStyleSheet(qss);
  }
}

// ----- API publique -----

void ThemeManager::applyLightD(QApplication &app, bool sunlight) {
  m_dark = false;
  m_sunlight = sunlight;
  applyPaletteLight_D(app);
  applyQss(app, ":/qss/theme_light_D.qss", sunlight);
  emit themeChanged();
}

void ThemeManager::applyDarkC(QApplication &app, bool sunlight) {
  m_dark = true;
  m_sunlight = sunlight;
  applyPaletteDark_C(app);
  applyQss(app, ":/qss/theme_dark_C.qss", sunlight);
  emit themeChanged();
}

void ThemeManager::setSunlight(QApplication &app, bool sunlight) {
  if (m_dark)
    applyDarkC(app, sunlight);
  else
    applyLightD(app, sunlight);
}
