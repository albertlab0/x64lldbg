#include "ScriptEditorDialog.h"
#include "core/DebugCore.h"
#include "common/Configuration.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

ScriptEditorDialog::ScriptEditorDialog(DebugCore* debugCore, QWidget* parent)
    : QDialog(parent)
    , m_debugCore(debugCore)
{
    setWindowTitle("Script Editor — Python (LLDB)");
    resize(700, 550);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // Toolbar
    auto* toolLayout = new QHBoxLayout();
    toolLayout->setSpacing(4);

    auto* runBtn = new QPushButton("Run", this);
    runBtn->setToolTip("Execute the script (Ctrl+Enter)");
    auto* loadBtn = new QPushButton("Load...", this);
    loadBtn->setToolTip("Load script from file");
    auto* saveBtn = new QPushButton("Save...", this);
    saveBtn->setToolTip("Save script to file");
    auto* clearBtn = new QPushButton("Clear", this);
    clearBtn->setToolTip("Clear editor and output");

    m_statusLabel = new QLabel("Ready", this);

    toolLayout->addWidget(runBtn);
    toolLayout->addWidget(loadBtn);
    toolLayout->addWidget(saveBtn);
    toolLayout->addWidget(clearBtn);
    toolLayout->addStretch();
    toolLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(toolLayout);

    // Splitter: editor on top, output on bottom
    auto* splitter = new QSplitter(Qt::Vertical, this);

    m_editor = new QPlainTextEdit(this);
    m_editor->setPlaceholderText(
        "# Python script (LLDB Python API)\n"
        "# Available: lldb.debugger, lldb.target, lldb.process, lldb.thread, lldb.frame\n"
        "#\n"
        "# Example:\n"
        "#   for reg in lldb.frame.registers[0]:\n"
        "#       print(f\"{reg.name} = {reg.value}\")\n"
    );
    m_editor->setTabStopDistance(32);

    m_outputPane = new QPlainTextEdit(this);
    m_outputPane->setReadOnly(true);
    m_outputPane->setMaximumBlockCount(5000);

    splitter->addWidget(m_editor);
    splitter->addWidget(m_outputPane);
    splitter->setStretchFactor(0, 3);  // editor gets more space
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter, 1);

    applyStyle();

    // Connections
    connect(runBtn, &QPushButton::clicked, this, &ScriptEditorDialog::runScript);
    connect(loadBtn, &QPushButton::clicked, this, &ScriptEditorDialog::loadScript);
    connect(saveBtn, &QPushButton::clicked, this, &ScriptEditorDialog::saveScript);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_editor->clear();
        m_outputPane->clear();
        m_statusLabel->setText("Cleared");
    });

    // Ctrl+Enter to run
    auto* runAction = new QAction(this);
    runAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    connect(runAction, &QAction::triggered, this, &ScriptEditorDialog::runScript);
    addAction(runAction);
}

void ScriptEditorDialog::applyStyle()
{
    QFont font = ConfigFont("Disassembly");
    m_editor->setFont(font);
    m_outputPane->setFont(font);

    QColor bg = ConfigColor("DisassemblyBackgroundColor");
    QColor fg = ConfigColor("DisassemblyTextColor");
    QColor border = ConfigColor("ChromeBorderColor");
    QColor surface = ConfigColor("ChromeSurfaceColor");
    QColor accent = ConfigColor("ChromeAccentColor");

    m_editor->setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; border: 1px solid %3; }"
    ).arg(bg.name(), fg.name(), border.name()));

    m_outputPane->setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; border: 1px solid %3; }"
    ).arg(surface.name(), fg.name(), border.name()));

    m_statusLabel->setStyleSheet(QString("QLabel { color: %1; }").arg(fg.name()));

    setStyleSheet(QString(
        "QDialog { background-color: %1; }"
        "QPushButton { background-color: %2; color: %3; border: 1px solid %4;"
        "  border-radius: 3px; padding: 4px 12px; }"
        "QPushButton:hover { background-color: %4; }"
    ).arg(bg.name(), surface.name(), fg.name(), border.name()));
}

void ScriptEditorDialog::runScript()
{
    QString code = m_editor->toPlainText().trimmed();
    if (code.isEmpty()) return;

    m_statusLabel->setText("Running...");
    m_outputPane->appendPlainText(">>> Running script...");

    QString output, error;
    bool ok = m_debugCore->executeScript(code, output, error);

    if (!output.isEmpty()) {
        m_outputPane->appendPlainText(output);
        emit outputProduced(output);
    }
    if (!error.isEmpty()) {
        m_outputPane->appendHtml(QString("<span style='color: #E53E3E;'>%1</span>")
            .arg(error.toHtmlEscaped()));
        emit errorProduced(error);
    }

    m_statusLabel->setText(ok ? "Completed" : "Error");
}

void ScriptEditorDialog::loadScript()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Load Script", QString(),
        "Python Scripts (*.py);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
            QString("Cannot open file: %1").arg(file.errorString()));
        return;
    }

    QTextStream in(&file);
    m_editor->setPlainText(in.readAll());
    m_statusLabel->setText("Loaded: " + path);
}

void ScriptEditorDialog::saveScript()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save Script", QString(),
        "Python Scripts (*.py);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
            QString("Cannot save file: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << m_editor->toPlainText();
    m_statusLabel->setText("Saved: " + path);
}
