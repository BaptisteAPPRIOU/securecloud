#include <QApplication>
#include <QSignalSpy>
#include <QtTest>
#include "../src/ThemeManager.h"

class TestThemeManager : public QObject {
    Q_OBJECT
private slots:
    void testApplyThemes() {
        ThemeManager tm;
        QSignalSpy spy(&tm, &ThemeManager::themeChanged);

        // Need a QApplication for palette/style changes
        // The test main creates QApplication.

        tm.applyLightD(*qApp, false);
        QCOMPARE(tm.isDark(), false);
        QCOMPARE(tm.sunlight(), false);
        QCOMPARE(spy.count(), 1);

        tm.applyDarkC(*qApp, true);
        QCOMPARE(tm.isDark(), true);
        QCOMPARE(tm.sunlight(), true);
        QCOMPARE(spy.count(), 2);

        tm.setSunlight(*qApp, false);
        // setSunlight triggers applyLightD/applyDarkC depending on current state
        QCOMPARE(tm.sunlight(), false);
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    TestThemeManager tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_thememanager.moc"
