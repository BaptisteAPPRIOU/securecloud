#pragma once
#include <QWidget>
#include <QVector>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui { class HomePage; }
QT_END_NAMESPACE

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(QWidget* parent = nullptr);
    ~HomePage();

    void setCurrentUserEmail(const QString& email);

signals:
    void requestLogout();

private:
    struct Conversation {
        QString title;
        QString preview;
        QString description;
        QStringList members;
        QStringList messages;
    };

    void seedDemoData();
    void refreshConversationList(const QString& filter = QString());
    void showConversationFromVisibleRow(int row);
    void renderConversation(int conversationIndex);
    void updateConversationPreview(int conversationIndex, const QString& newPreview);

    Ui::HomePage* ui;
    QVector<Conversation> m_conversations;
    QVector<int> m_visibleIndexes;
    int m_currentConversation = -1;
    QString m_currentUserEmail;
};
