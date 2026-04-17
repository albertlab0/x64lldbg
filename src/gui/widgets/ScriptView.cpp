#include "ScriptView.h"
#include "core/DebugCore.h"
#include "common/Configuration.h"
#include "gui/dialogs/ScriptEditorDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QKeyEvent>
#include <QMenu>
#include <QLabel>

ScriptView::ScriptView(DebugCore* debugCore, QWidget* parent)
    : QWidget(parent)
    , m_debugCore(debugCore)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Output pane (read-only)
    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(10000);  // prevent unbounded growth
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_output, 1);

    // Input bar at the bottom
    auto* inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(4, 2, 4, 2);
    inputLayout->setSpacing(4);

    auto* promptLabel = new QLabel(">>>", this);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("LLDB command or script <expression> (e.g., script print(lldb.frame))");

    m_editorBtn = new QPushButton("Editor...", this);
    m_editorBtn->setToolTip("Open multi-line script editor");
    m_editorBtn->setFixedWidth(70);

    inputLayout->addWidget(promptLabel);
    inputLayout->addWidget(m_input, 1);
    inputLayout->addWidget(m_editorBtn);
    layout->addLayout(inputLayout);

    applyStyle();

    // Execute on Enter
    connect(m_input, &QLineEdit::returnPressed, this, &ScriptView::executeInput);

    // Editor button opens the multi-line script editor
    connect(m_editorBtn, &QPushButton::clicked, this, [this]() {
        ScriptEditorDialog dlg(m_debugCore, this);
        connect(&dlg, &ScriptEditorDialog::outputProduced,
                this, &ScriptView::appendOutput);
        connect(&dlg, &ScriptEditorDialog::errorProduced,
                this, &ScriptView::appendError);
        dlg.exec();
    });

    // History navigation via Up/Down arrow keys
    m_input->installEventFilter(this);

    // Initial banner
    appendOutput("LLDB Script Console");
    appendOutput("Type LLDB commands (e.g., 'bt', 'register read') or use 'script' for Python.");
    appendOutput("Use 'script <python>' for one-liners, or click 'Editor...' for multi-line scripts.");
    appendOutput("");

    setupContextMenu();
}

void ScriptView::applyStyle()
{
    QFont font = ConfigFont("Log");
    m_output->setFont(font);
    m_input->setFont(font);
    m_editorBtn->setFont(font);

    QColor bg = ConfigColor("DisassemblyBackgroundColor");
    QColor fg = ConfigColor("DisassemblyTextColor");
    QColor border = ConfigColor("ChromeBorderColor");
    QColor accent = ConfigColor("ChromeAccentColor");
    QColor surface = ConfigColor("ChromeSurfaceColor");

    m_output->setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; border: none; }"
    ).arg(bg.name(), fg.name()));

    m_input->setStyleSheet(QString(
        "QLineEdit { background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 3px 6px; }"
        "QLineEdit:focus { border: 1px solid %4; }"
    ).arg(bg.name(), fg.name(), border.name(), accent.name()));

    m_editorBtn->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; padding: 3px 8px; }"
        "QPushButton:hover { background-color: %3; }"
    ).arg(surface.name(), fg.name(), border.name()));

    // Prompt label style
    if (auto* promptLabel = findChild<QLabel*>()) {
        promptLabel->setFont(font);
        promptLabel->setStyleSheet(QString("QLabel { color: %1; }").arg(accent.name()));
    }

    setStyleSheet(QString("QWidget { background-color: %1; }").arg(bg.name()));
}

void ScriptView::setupContextMenu()
{
    m_output->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_output, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction("Clear Output", this, [this]() {
            m_output->clear();
        });
        menu.addSeparator();
        menu.addAction("Copy", m_output, &QPlainTextEdit::copy);
        menu.addAction("Select All", m_output, &QPlainTextEdit::selectAll);
        menu.exec(m_output->viewport()->mapToGlobal(pos));
    });
}

void ScriptView::executeInput()
{
    QString cmd = m_input->text().trimmed();
    if (cmd.isEmpty()) return;

    // Add to history
    if (m_history.isEmpty() || m_history.last() != cmd) {
        m_history.append(cmd);
        if (m_history.size() > MAX_HISTORY)
            m_history.removeFirst();
    }
    m_historyIndex = -1;

    // Show the command in the output
    appendOutput(">>> " + cmd);

    // Execute via DebugCore
    QString output, error;
    m_debugCore->executeCommand(cmd, output, error);

    if (!output.isEmpty())
        appendOutput(output);
    if (!error.isEmpty())
        appendError(error);

    m_input->clear();
}

void ScriptView::appendOutput(const QString& text)
{
    m_output->appendPlainText(text);
    // Auto-scroll to bottom
    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_output->setTextCursor(cursor);
}

void ScriptView::appendError(const QString& text)
{
    // Errors shown in red via HTML
    m_output->appendHtml(QString("<span style='color: #E53E3E;'>%1</span>")
        .arg(text.toHtmlEscaped()));
    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_output->setTextCursor(cursor);
}

void ScriptView::historyUp()
{
    if (m_history.isEmpty()) return;
    if (m_historyIndex < 0)
        m_historyIndex = m_history.size() - 1;
    else if (m_historyIndex > 0)
        m_historyIndex--;
    m_input->setText(m_history[m_historyIndex]);
}

void ScriptView::historyDown()
{
    if (m_history.isEmpty() || m_historyIndex < 0) return;
    if (m_historyIndex < m_history.size() - 1) {
        m_historyIndex++;
        m_input->setText(m_history[m_historyIndex]);
    } else {
        m_historyIndex = -1;
        m_input->clear();
    }
}

// Event filter for Up/Down arrow history navigation in the input line
bool ScriptView::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            historyUp();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down) {
            historyDown();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
