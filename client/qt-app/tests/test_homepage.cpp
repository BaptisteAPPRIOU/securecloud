#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QtTest>
#include "../src/homepage.h"

class TestHomePage : public QObject {
    Q_OBJECT
private slots:
    void testConversationDisplayAndSearch() {
        HomePage w;
        w.show();

        auto* list = w.findChild<QListWidget*>("listConversations");
        QVERIFY(list);
        QVERIFY(list->count() > 0);

        list->setCurrentRow(0);
        auto* title = w.findChild<QLabel*>("lblConversationTitle");
        QVERIFY(title);
        QVERIFY(!title->text().isEmpty());

        auto* search = w.findChild<QLineEdit*>("editSearch");
        QVERIFY(search);
        search->setText("Weekly");
        QVERIFY(list->count() >= 1);
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    TestHomePage tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_homepage.moc"
