#include "CPURegistersView.h"
#include "common/Configuration.h"

#include <QHeaderView>

CPURegistersView::CPURegistersView(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    refresh();
}

void CPURegistersView::setupColumns()
{
    setColumnCount(3);
    horizontalHeader()->setVisible(false);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setShowGrid(false);
}

void CPURegistersView::applyStyle()
{
    QFont font = ConfigFont("Registers");
    setFont(font);

    QColor bg = ConfigColor("RegistersBackgroundColor");
    QColor fg = ConfigColor("RegistersTextColor");
    QColor sel = ConfigColor("DisassemblySelectionColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: none; outline: none; }"
        "QTableWidget::item { padding: 0 4px; }"
        "QTableWidget::item:selected { background-color: %3; }"
    ).arg(bg.name(), fg.name(), sel.name()));
}

void CPURegistersView::refresh()
{
    auto registers = m_debugCore->getRegisters();
    setRowCount(registers.size());

    QColor labelColor = ConfigColor("RegistersLabelColor");
    QColor valueColor = ConfigColor("RegistersTextColor");
    QColor modColor = ConfigColor("RegistersModifiedColor");
    QColor strRefColor = ConfigColor("StringReferenceColor");
    QColor symRefColor = ConfigColor("SymbolReferenceColor");

    for (int i = 0; i < registers.size(); i++) {
        const auto& reg = registers[i];

        auto* nameItem = new QTableWidgetItem(reg.name);
        nameItem->setForeground(labelColor);
        setItem(i, 0, nameItem);

        auto* valueItem = new QTableWidgetItem(
            QString("0x%1").arg(reg.value, 16, 16, QChar('0'))
        );
        valueItem->setForeground(reg.modified ? modColor : valueColor);
        setItem(i, 1, valueItem);

        // Smart dereference column — show string/symbol if pointer
        QString deref;
        if (reg.name == "RSI") {
            // Stub: demonstrate string dereference
            deref = "\"Hello, World!\"";
        } else if (reg.name == "RIP") {
            deref = "<main>";
        } else if (reg.value > 0x1000 && reg.name != "RFLAGS") {
            deref = m_debugCore->dereferencePointer(reg.value);
        }

        auto* refItem = new QTableWidgetItem(deref);
        refItem->setForeground(deref.startsWith('"') ? strRefColor : symRefColor);
        setItem(i, 2, refItem);
    }
}
