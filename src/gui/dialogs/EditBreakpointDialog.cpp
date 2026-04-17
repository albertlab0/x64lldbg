#include "EditBreakpointDialog.h"
#include "common/Configuration.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QGroupBox>

EditBreakpointDialog::EditBreakpointDialog(DebugCore* debugCore,
                                           const BreakpointInfo& bp,
                                           QWidget* parent)
    : QDialog(parent)
    , m_debugCore(debugCore)
{
    setWindowTitle(QString("Edit Breakpoint — 0x%1").arg(bp.address, 0, 16));
    setMinimumWidth(500);
    setupUI(bp);
}

void EditBreakpointDialog::setupUI(const BreakpointInfo& bp)
{
    auto* mainLayout = new QVBoxLayout(this);

    // Info header
    auto* infoLabel = new QLabel(
        QString("<b>Address:</b> 0x%1 &nbsp; <b>Module:</b> %2 &nbsp; "
                "<b>Hit Count:</b> %3")
            .arg(bp.address, 0, 16)
            .arg(bp.module.isEmpty() ? "—" : bp.module)
            .arg(bp.hitCount));
    mainLayout->addWidget(infoLabel);
    mainLayout->addSpacing(8);

    // Break condition
    auto* breakGroup = new QGroupBox("Break Condition");
    auto* breakLayout = new QVBoxLayout(breakGroup);
    auto* breakHint = new QLabel(
        "Expression that must evaluate to true (non-zero) to trigger the break.\n"
        "Leave empty to always break. Uses LLDB expression syntax.");
    breakHint->setWordWrap(true);
    breakHint->setStyleSheet("color: #888;");
    m_editBreakCondition = new QLineEdit(bp.condition);
    m_editBreakCondition->setPlaceholderText("e.g., $rax == 0 || $rcx > 100");
    m_editBreakCondition->setFont(ConfigFont("Disassembly"));
    breakLayout->addWidget(breakHint);
    breakLayout->addWidget(m_editBreakCondition);
    mainLayout->addWidget(breakGroup);

    // Log
    auto* logGroup = new QGroupBox("Log");
    auto* logLayout = new QFormLayout(logGroup);
    auto* logHint = new QLabel(
        "Text logged when breakpoint is hit. Use {register} for values.\n"
        "Examples: {rax}, {rip}, {s:rdi} (string at rdi), {d:rcx} (decimal)");
    logHint->setWordWrap(true);
    logHint->setStyleSheet("color: #888;");
    logLayout->addRow(logHint);

    m_editLogText = new QLineEdit(bp.logText);
    m_editLogText->setPlaceholderText("e.g., rax={rax} rdi={s:rdi}");
    m_editLogText->setFont(ConfigFont("Disassembly"));
    logLayout->addRow("Log Text:", m_editLogText);

    m_editLogCondition = new QLineEdit(bp.logCondition);
    m_editLogCondition->setPlaceholderText("Leave empty to always log");
    m_editLogCondition->setFont(ConfigFont("Disassembly"));
    logLayout->addRow("Log Condition:", m_editLogCondition);
    mainLayout->addWidget(logGroup);

    // Options
    m_checkFastResume = new QCheckBox("Fast Resume (log only, do not break)");
    m_checkFastResume->setChecked(bp.fastResume);
    mainLayout->addWidget(m_checkFastResume);

    mainLayout->addSpacing(8);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* okBtn = new QPushButton("OK");
    auto* cancelBtn = new QPushButton("Cancel");
    okBtn->setDefault(true);
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QString EditBreakpointDialog::breakCondition() const
{
    return m_editBreakCondition->text().trimmed();
}

QString EditBreakpointDialog::logText() const
{
    return m_editLogText->text().trimmed();
}

QString EditBreakpointDialog::logCondition() const
{
    return m_editLogCondition->text().trimmed();
}

bool EditBreakpointDialog::fastResume() const
{
    return m_checkFastResume->isChecked();
}
