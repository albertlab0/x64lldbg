#include "CPUArgumentWidget.h"
#include "common/Configuration.h"

#include <QHeaderView>

// AMD64 System V ABI: first 6 integer/pointer args in registers
static const char* const kArgRegs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
static const int kNumArgRegs = 6;

CPUArgumentWidget::CPUArgumentWidget(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    refresh();
}

void CPUArgumentWidget::setupColumns()
{
    setColumnCount(3);
    setHorizontalHeaderLabels({"Arg", "Register", "Value"});

    horizontalHeader()->setVisible(true);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    horizontalHeader()->setStretchLastSection(true);

    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setShowGrid(false);
}

void CPUArgumentWidget::applyStyle()
{
    QFont font = ConfigFont("Default");
    setFont(font);

    QColor bg = ConfigColor("DisassemblyBackgroundColor");
    QColor fg = ConfigColor("DisassemblyTextColor");
    QColor grid = ConfigColor("TableGridColor");
    QColor sel = ConfigColor("DisassemblySelectionColor");
    QColor hdrBg = ConfigColor("TableHeaderBackgroundColor");
    QColor hdrFg = ConfigColor("TableHeaderTextColor");
    QColor border = ConfigColor("ChromeBorderColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: none; outline: none; }"
        "QTableWidget::item { padding: 0 4px; }"
        "QTableWidget::item:selected { background-color: %3; }"
        "QHeaderView::section { background-color: %4; color: %5; border: none;"
        "  border-right: 1px solid %6; border-bottom: 1px solid %6;"
        "  padding: 2px 4px; font-size: 12px; font-weight: 500; }"
    ).arg(bg.name(), fg.name(), sel.name(), hdrBg.name(), hdrFg.name(), border.name()));
}

QString CPUArgumentWidget::formatValue(uint64_t value)
{
    QString result = QString("0x%1").arg(value, 0, 16);

    // Try to dereference as string
    QString str = m_debugCore->getStringAt(value);
    if (!str.isEmpty()) {
        result += QString(" \"%1\"").arg(str.left(64));
        return result;
    }

    // Try symbol
    QString sym = m_debugCore->getSymbolAt(value);
    if (!sym.isEmpty()) {
        result += QString(" <%1>").arg(sym);
        return result;
    }

    return result;
}

void CPUArgumentWidget::refresh()
{
    auto regs = m_debugCore->getRegisters();

    // Build a lookup map for register values
    QMap<QString, uint64_t> regMap;
    for (const auto& reg : regs)
        regMap[reg.name.toLower()] = reg.value;

    // Count rows: 6 register args + header label + stack args
    // We show up to 4 stack arguments beyond the 6 register args
    uint64_t rsp = regMap.value("rsp", 0);

    // Determine how many stack args to show (peek at stack)
    int stackArgCount = 4; // show 4 potential stack args
    int totalRows = kNumArgRegs + 1 + stackArgCount; // +1 for "Stack args" separator
    setRowCount(totalRows);

    QColor nameColor = ConfigColor("RegistersLabelColor");
    QColor regColor = ConfigColor("DisassemblyAddressColor");
    QColor valueColor = ConfigColor("RegistersTextColor");
    QColor commentColor = ConfigColor("DisassemblyCommentColor");

    // Register arguments (arg0-arg5)
    for (int i = 0; i < kNumArgRegs; i++) {
        QString regName = QString(kArgRegs[i]);
        uint64_t val = regMap.value(regName, 0);

        auto* argItem = new QTableWidgetItem(QString("arg%1").arg(i));
        argItem->setForeground(nameColor);
        setItem(i, 0, argItem);

        auto* regItem = new QTableWidgetItem(regName.toUpper());
        regItem->setForeground(regColor);
        setItem(i, 1, regItem);

        auto* valItem = new QTableWidgetItem(formatValue(val));
        valItem->setForeground(valueColor);
        setItem(i, 2, valItem);
    }

    // Separator row: "Stack arguments (RSP+0x8)"
    int sepRow = kNumArgRegs;
    auto* sepItem = new QTableWidgetItem(
        QString("Stack args @ RSP+0x8 (0x%1)").arg(rsp + 8, 0, 16));
    sepItem->setForeground(commentColor);
    setItem(sepRow, 0, sepItem);
    setSpan(sepRow, 0, 1, 3);

    // Stack arguments: at [rsp+8], [rsp+16], [rsp+24], [rsp+32]
    // (rsp+0 is the return address)
    if (rsp != 0) {
        QByteArray stackData = m_debugCore->readMemory(rsp + 8, stackArgCount * 8);
        for (int i = 0; i < stackArgCount; i++) {
            int row = sepRow + 1 + i;
            int offset = 8 + i * 8;

            auto* argItem = new QTableWidgetItem(QString("arg%1").arg(kNumArgRegs + i));
            argItem->setForeground(nameColor);
            setItem(row, 0, argItem);

            auto* locItem = new QTableWidgetItem(
                QString("[RSP+0x%1]").arg(offset, 0, 16));
            locItem->setForeground(regColor);
            setItem(row, 1, locItem);

            uint64_t val = 0;
            if (stackData.size() >= (i + 1) * 8) {
                memcpy(&val, stackData.constData() + i * 8, 8);
            }

            auto* valItem = new QTableWidgetItem(formatValue(val));
            valItem->setForeground(valueColor);
            setItem(row, 2, valItem);
        }
    }
}
