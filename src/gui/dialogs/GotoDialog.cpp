#include "GotoDialog.h"
#include "core/DebugCore.h"
#include "common/Configuration.h"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>

GotoDialog::GotoDialog(DebugCore* debugCore, QWidget* parent)
    : QDialog(parent)
    , m_debugCore(debugCore)
{
    setWindowTitle("Go to Address");
    setMinimumWidth(460);
    setFixedHeight(120);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText("Address, symbol, or expression (e.g. main+0x10)");
    m_lineEdit->setFont(ConfigFont("Disassembly"));

    m_statusLabel = new QLabel(this);
    QFont statusFont = ConfigFont("Disassembly");
    statusFont.setPointSize(statusFont.pointSize() - 1);
    m_statusLabel->setFont(statusFont);

    m_okButton = new QPushButton("Go", this);
    m_okButton->setEnabled(false);
    m_okButton->setDefault(true);
    auto* cancelButton = new QPushButton("Cancel", this);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(cancelButton);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 12);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(m_lineEdit);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(buttonLayout);

    // Theme-aware styling
    applyStyle();

    connect(m_lineEdit, &QLineEdit::textChanged, this, &GotoDialog::onTextChanged);
    connect(m_okButton, &QPushButton::clicked, this, &GotoDialog::onOkClicked);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_lineEdit, &QLineEdit::returnPressed, this, &GotoDialog::onOkClicked);
}

void GotoDialog::onTextChanged(const QString& text)
{
    if (text.trimmed().isEmpty()) {
        m_statusLabel->setText("");
        m_okButton->setEnabled(false);
        return;
    }

    bool ok = false;
    uint64_t addr = evaluateExpression(text.trimmed(), &ok);

    if (ok) {
        // Try to resolve symbol name at this address for display
        QString sym = m_debugCore->getSymbolAt(addr);
        QString display = QString("= 0x%1").arg(addr, 0, 16);
        if (!sym.isEmpty())
            display += QString("  <%1>").arg(sym);
        m_statusLabel->setText(display);
        m_statusLabel->setStyleSheet("QLabel { color: #167C3D; }");
        m_resultAddress = addr;
        m_okButton->setEnabled(true);
    } else {
        m_statusLabel->setText("Invalid expression");
        m_statusLabel->setStyleSheet("QLabel { color: #E53E3E; }");
        m_okButton->setEnabled(false);
    }
}

void GotoDialog::onOkClicked()
{
    if (m_okButton->isEnabled())
        accept();
}

// Evaluate an expression like "main+0x10", "hello+0x11a1", "0x401000", "rip"
uint64_t GotoDialog::evaluateExpression(const QString& expr, bool* ok)
{
    *ok = false;
    QString s = expr.trimmed();
    if (s.isEmpty())
        return 0;

    // Split on + or - while keeping the operator
    // Supports: "symbol+0x10", "symbol-0x20", "0x401000", "main", "rsp+8"
    // Tokenize: split into (sign, term) pairs
    // First term has implicit +
    QVector<QPair<QChar, QString>> terms;
    QString current;
    QChar currentSign = '+';

    for (int i = 0; i < s.size(); i++) {
        QChar c = s[i];
        if ((c == '+' || c == '-') && i > 0) {
            if (!current.isEmpty()) {
                terms.append({currentSign, current.trimmed()});
                current.clear();
            }
            currentSign = c;
        } else {
            current += c;
        }
    }
    if (!current.trimmed().isEmpty())
        terms.append({currentSign, current.trimmed()});

    if (terms.isEmpty())
        return 0;

    uint64_t result = 0;
    for (const auto& term : terms) {
        bool termOk = false;
        uint64_t val = 0;
        QString t = term.second;

        // Try as hex number (0x... or plain hex)
        if (t.startsWith("0x", Qt::CaseInsensitive)) {
            val = t.mid(2).toULongLong(&termOk, 16);
        }

        // Try as plain decimal number
        if (!termOk) {
            val = t.toULongLong(&termOk, 10);
        }

        // Try as plain hex (no 0x prefix, but looks like hex)
        if (!termOk) {
            val = t.toULongLong(&termOk, 16);
        }

        // Try as register name
        if (!termOk) {
            auto regs = m_debugCore->getRegisters();
            for (const auto& reg : regs) {
                if (reg.name.compare(t, Qt::CaseInsensitive) == 0) {
                    val = reg.value;
                    termOk = true;
                    break;
                }
            }
        }

        // Try as symbol (function name like "main", "printf")
        if (!termOk) {
            val = resolveSymbol(t, &termOk);
        }

        // Try as module base (module name without a symbol)
        if (!termOk) {
            val = resolveModuleBase(t, &termOk);
        }

        if (!termOk)
            return 0;

        if (term.first == '+')
            result += val;
        else
            result -= val;
    }

    *ok = true;
    return result;
}

uint64_t GotoDialog::resolveSymbol(const QString& name, bool* ok)
{
    *ok = false;

#ifdef HAS_LLDB
    // Use LLDB to find the symbol address
    // This handles function names, labels, etc.
    auto modules = m_debugCore->getMemoryMap();

    // Try "module.symbol" format (e.g., "hello.main" or just "main")
    // Also handles "module+offset" where module was already split by caller

    // First try to find the symbol directly via LLDB
    uint64_t addr = m_debugCore->findSymbolAddress(name);
    if (addr != 0) {
        *ok = true;
        return addr;
    }
#endif

    return 0;
}

uint64_t GotoDialog::resolveModuleBase(const QString& moduleName, bool* ok)
{
    *ok = false;

    auto regions = m_debugCore->getMemoryMap();
    for (const auto& region : regions) {
        // Match module name (case insensitive, with or without path)
        QString modName = region.name;
        // Extract just the filename from the path
        int lastSlash = modName.lastIndexOf('/');
        if (lastSlash >= 0)
            modName = modName.mid(lastSlash + 1);

        if (modName.compare(moduleName, Qt::CaseInsensitive) == 0 ||
            modName.startsWith(moduleName + ".", Qt::CaseInsensitive) ||
            modName.startsWith(moduleName + "-", Qt::CaseInsensitive)) {
            *ok = true;
            return region.base;
        }
    }

    return 0;
}

void GotoDialog::applyStyle()
{
    QColor surface = ConfigColor("ChromeSurfaceColor");
    QColor bg      = ConfigColor("DisassemblyBackgroundColor");
    QColor border  = ConfigColor("ChromeBorderColor");
    QColor fg      = ConfigColor("DisassemblyTextColor");
    QColor accent  = ConfigColor("ChromeAccentColor");
    QColor hover   = ConfigColor("ChromeHoverColor");
    QColor sel     = ConfigColor("DisassemblySelectionColor");

    setStyleSheet(QString(
        "QDialog { background-color: %1; }"
        "QLineEdit { background-color: %2; border: 1px solid %3; border-radius: 6px;"
        "  padding: 8px 12px; font-size: 13px; color: %4; selection-background-color: %7; }"
        "QLineEdit:focus { border: 1px solid %5; }"
        "QPushButton { background-color: %6; border: 1px solid %3; border-radius: 6px;"
        "  padding: 6px 20px; font-size: 13px; color: %4; }"
        "QPushButton:hover { background-color: %3; }"
        "QPushButton:default { background-color: %5; color: #FFFFFF; border: none; }"
        "QPushButton:default:hover { background-color: %5; }"
        "QLabel { font-size: 12px; color: %4; }"
    ).arg(
        surface.name(),   // %1
        bg.name(),        // %2
        border.name(),    // %3
        fg.name(),        // %4
        accent.name(),    // %5
        hover.name(),     // %6
        sel.name()        // %7
    ));
}
