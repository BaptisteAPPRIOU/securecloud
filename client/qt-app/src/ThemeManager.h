#pragma once
#include <QApplication>
#include <QObject>

class ThemeManager : public QObject {
  Q_OBJECT
public:
  explicit ThemeManager(QObject *parent = nullptr) : QObject(parent) {}

  void applyLightD(QApplication &app, bool sunlight);
  void applyDarkC(QApplication &app, bool sunlight);
  void setSunlight(QApplication &app, bool sunlight);

  bool isDark() const { return m_dark; }
  bool sunlight() const { return m_sunlight; }

signals:
  void themeChanged();

private:
  void applyQss(QApplication &app, const QString &baseQss, bool sunlight);

  bool m_dark = false;
  bool m_sunlight = false;
};