#include <QApplication>
#include <QPushButton>
#include <QLineEdit>
#include <QSignalSpy>
#include <QtTest>
#include "../src/loginpage.h"

class TestLogin : public QObject {
    Q_OBJECT
private slots:
    void testSubmitAndForgot() {
        LoginPage w;
        w.show();

        QSignalSpy spyForgot(&w, &LoginPage::requestForgot);
        QSignalSpy spySubmit(&w, &LoginPage::requestLoginSubmit);

        QPushButton *linkForgot = w.findChild<QPushButton *>("linkForgot");
        QVERIFY(linkForgot);
        QTest::mouseClick(linkForgot, Qt::LeftButton);
        QCOMPARE(spyForgot.count(), 1);

        QLineEdit *editEmail = w.findChild<QLineEdit *>("editEmail");
        QLineEdit *editPwd = w.findChild<QLineEdit *>("editPwd");
        QVERIFY(editEmail);
        QVERIFY(editPwd);

        editEmail->setText("test@example.com");
        editPwd->setText("Secret123");

        QPushButton *btnLogin = w.findChild<QPushButton *>("btnLogin");
        QVERIFY(btnLogin);
        QTest::mouseClick(btnLogin, Qt::LeftButton);
        QCOMPARE(spySubmit.count(), 1);

        // verify arguments
        QList<QVariant> args = spySubmit.takeFirst();
        QCOMPARE(args.at(0).toString(), QString("test@example.com"));
        QCOMPARE(args.at(1).toString(), QString("Secret123"));
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    TestLogin tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_login.moc"
