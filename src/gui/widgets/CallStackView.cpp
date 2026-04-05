#include "CallStackView.h"
#include "common/Configuration.h"

#include <QHeaderView>

CallStackView::CallStackView(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    refresh();
}

void CallStackView::setupColumns()
{
    setColumnCount(5);
    setHorizontalHeaderLabels({"#", "Address", "Return To", "Module", "Function"});
    horizontalHeader()->setStretchLastSection(true);
    for (int i = 0; i < 4; i++)
        horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setShowGrid(false);
    setAlternatingRowColors(true);
}

void CallStackView::applyStyle()
{
    QFont font = ConfigFont("Default");
    setFont(font);

    QColor bg = ConfigColor("DisassemblyBackgroundColor");
    QColor fg = ConfigColor("DisassemblyTextColor");
    QColor grid = ConfigColor("TableGridColor");
    QColor alt = ConfigColor("AlternateRowColor");
    QColor hdrBg = ConfigColor("TableHeaderBackgroundColor");
    QColor hdrFg = ConfigColor("TableHeaderTextColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; gridline-color: %3; }"
        "QTableWidget::item:alternate { background-color: %4; }"
        "QHeaderView::section { background-color: %5; color: %6; border: 1px solid %3; padding: 2px; }"
    ).arg(bg.name(), fg.name(), grid.name(), alt.name(), hdrBg.name(), hdrFg.name()));
}

void CallStackView::refresh()
{
    auto stack = m_debugCore->getCallStack();
    setRowCount(stack.size());

    QColor addrColor = ConfigColor("DisassemblyAddressColor");
    QColor symColor = ConfigColor("SymbolReferenceColor");

    for (int i = 0; i < stack.size(); i++) {
        const auto& entry = stack[i];
        setItem(i, 0, new QTableWidgetItem(QString::number(entry.index)));

        auto* addrItem = new QTableWidgetItem(QString("0x%1").arg(entry.address, 16, 16, QChar('0')));
        addrItem->setForeground(addrColor);
        setItem(i, 1, addrItem);

        auto* retItem = new QTableWidgetItem(QString("0x%1").arg(entry.returnAddress, 16, 16, QChar('0')));
        retItem->setForeground(addrColor);
        setItem(i, 2, retItem);

        setItem(i, 3, new QTableWidgetItem(entry.module));

        auto* funcItem = new QTableWidgetItem(entry.function);
        funcItem->setForeground(symColor);
        setItem(i, 4, funcItem);
    }
}
