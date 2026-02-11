#include "homepage.h"
#include "ui_homepage.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>

namespace {
QString conversationItemText(const QString& title, const QString& preview) {
    if (preview.isEmpty()) {
        return title;
    }
    return QStringLiteral("%1\n%2").arg(title, preview);
}
}

HomePage::HomePage(QWidget* parent)
    : QWidget(parent), ui(new Ui::HomePage)
{
    ui->setupUi(this);
    seedDemoData();
    refreshConversationList();

    connect(ui->editSearch, &QLineEdit::textChanged, this, [this](const QString& text) {
        refreshConversationList(text);
    });
    connect(ui->listConversations, &QListWidget::currentRowChanged, this, [this](int row) {
        showConversationFromVisibleRow(row);
    });
    connect(ui->btnSend, &QPushButton::clicked, this, [this]() {
        if (m_currentConversation < 0 || m_currentConversation >= m_conversations.size()) {
            return;
        }

        const QString message = ui->editMessage->text().trimmed();
        if (message.isEmpty()) {
            return;
        }

        const QString author = m_currentUserEmail.isEmpty() ? QStringLiteral("You") : m_currentUserEmail;
        m_conversations[m_currentConversation].messages.append(QStringLiteral("%1: %2").arg(author, message));
        updateConversationPreview(m_currentConversation, message);

        ui->editMessage->clear();
        renderConversation(m_currentConversation);
        refreshConversationList(ui->editSearch->text());

        const int newRow = m_visibleIndexes.indexOf(m_currentConversation);
        if (newRow >= 0) {
            ui->listConversations->setCurrentRow(newRow);
        }
    });
    connect(ui->btnLogout, &QPushButton::clicked, this, &HomePage::requestLogout);

    if (!m_visibleIndexes.isEmpty()) {
        ui->listConversations->setCurrentRow(0);
    }
}

HomePage::~HomePage() { delete ui; }

void HomePage::setCurrentUserEmail(const QString& email)
{
    m_currentUserEmail = email.trimmed();
    if (m_currentConversation >= 0) {
        renderConversation(m_currentConversation);
    }
}

void HomePage::seedDemoData()
{
    m_conversations = {
        {
            QStringLiteral("Medical Team Alpha"),
            QStringLiteral("Can we validate the handover checklist?"),
            QStringLiteral("Operational channel used to coordinate field missions."),
            {QStringLiteral("Amina"), QStringLiteral("Luca"), QStringLiteral("Noah")},
            {
                QStringLiteral("Amina: We reached the clinic at 08:30."),
                QStringLiteral("Luca: Supplies are now in storage room B."),
                QStringLiteral("Noah: Can we validate the handover checklist?")
            }
        },
        {
            QStringLiteral("Emergency Logistics"),
            QStringLiteral("New shipment arrives at 17:00."),
            QStringLiteral("Live logistics updates for transport and stock."),
            {QStringLiteral("Maya"), QStringLiteral("Ethan"), QStringLiteral("Sofia"), QStringLiteral("Omar")},
            {
                QStringLiteral("Maya: Fuel delivery confirmed."),
                QStringLiteral("Ethan: New shipment arrives at 17:00."),
                QStringLiteral("Sofia: Copy, warehouse team informed.")
            }
        },
        {
            QStringLiteral("Weekly Planning"),
            QStringLiteral("Draft agenda uploaded."),
            QStringLiteral("Shared planning thread for the weekly steering meeting."),
            {QStringLiteral("Claire"), QStringLiteral("Hugo")},
            {
                QStringLiteral("Claire: Draft agenda uploaded."),
                QStringLiteral("Hugo: I'll review this afternoon.")
            }
        }
    };
}

void HomePage::refreshConversationList(const QString& filter)
{
    const QString needle = filter.trimmed();
    m_visibleIndexes.clear();
    ui->listConversations->clear();

    for (int index = 0; index < m_conversations.size(); ++index) {
        const Conversation& conversation = m_conversations.at(index);
        const bool match = needle.isEmpty()
            || conversation.title.contains(needle, Qt::CaseInsensitive)
            || conversation.preview.contains(needle, Qt::CaseInsensitive);
        if (!match) {
            continue;
        }

        m_visibleIndexes.append(index);
        auto* item = new QListWidgetItem(conversationItemText(conversation.title, conversation.preview));
        item->setData(Qt::UserRole, index);
        ui->listConversations->addItem(item);
    }

    if (m_visibleIndexes.isEmpty()) {
        m_currentConversation = -1;
        ui->lblConversationTitle->setText(QStringLiteral("No conversation"));
        ui->lblDescriptionValue->setText(QStringLiteral("No conversation matches this filter."));
        ui->listMembers->clear();
        ui->txtConversation->clear();
        ui->btnSend->setEnabled(false);
        ui->editMessage->setEnabled(false);
        return;
    }

    ui->btnSend->setEnabled(true);
    ui->editMessage->setEnabled(true);

    int nextRow = 0;
    if (m_currentConversation >= 0) {
        const int existingRow = m_visibleIndexes.indexOf(m_currentConversation);
        if (existingRow >= 0) {
            nextRow = existingRow;
        }
    }
    ui->listConversations->setCurrentRow(nextRow);
}

void HomePage::showConversationFromVisibleRow(int row)
{
    if (row < 0 || row >= m_visibleIndexes.size()) {
        return;
    }
    renderConversation(m_visibleIndexes.at(row));
}

void HomePage::renderConversation(int conversationIndex)
{
    if (conversationIndex < 0 || conversationIndex >= m_conversations.size()) {
        return;
    }
    m_currentConversation = conversationIndex;

    const Conversation& conversation = m_conversations.at(conversationIndex);
    ui->lblConversationTitle->setText(conversation.title);
    ui->lblDescriptionValue->setText(conversation.description);

    ui->listMembers->clear();
    for (const QString& member : conversation.members) {
        ui->listMembers->addItem(member);
    }

    ui->txtConversation->setPlainText(conversation.messages.join('\n'));
}

void HomePage::updateConversationPreview(int conversationIndex, const QString& newPreview)
{
    if (conversationIndex < 0 || conversationIndex >= m_conversations.size()) {
        return;
    }

    QString compact = newPreview;
    compact.replace('\n', ' ');
    if (compact.size() > 48) {
        compact = compact.left(45) + QStringLiteral("...");
    }
    m_conversations[conversationIndex].preview = compact;
}
