#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>
#include "../src/landingpage.h"

class TestLanding : public QObject {
    Q_OBJECT
private slots:
    void testButtons() {
        LandingPage w;
        w.show();

        QSignalSpy spyLogin(&w, &LandingPage::requestLogin);
        QSignalSpy spyForgot(&w, &LandingPage::requestForgot);

        QPushButton *btnLogin = w.findChild<QPushButton *>("btnGoLogin");
        QVERIFY(btnLogin);
        QTest::mouseClick(btnLogin, Qt::LeftButton);
        QCOMPARE(spyLogin.count(), 1);

        QPushButton *btnForgot = w.findChild<QPushButton *>("btnGoForgot");
        QVERIFY(btnForgot);
        QTest::mouseClick(btnForgot, Qt::LeftButton);
        QCOMPARE(spyForgot.count(), 1);
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    TestLanding tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_landing.moc"
