#include "CPUStack.h"
#include "common/Configuration.h"

#include <QHeaderView>

CPUStack::CPUStack(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    refresh();
}

void CPUStack::setupColumns()
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
    setShowGrid(false);
    setAlternatingRowColors(true);
}

void CPUStack::applyStyle()
{
    QFont font = ConfigFont("Stack");
    setFont(font);

    QColor bg = ConfigColor("StackBackgroundColor");
    QColor fg = ConfigColor("StackTextColor");
    QColor grid = ConfigColor("TableGridColor");
    QColor sel = ConfigColor("DisassemblySelectionColor");
    QColor alt = ConfigColor("AlternateRowColor");
    QColor hdrBg = ConfigColor("TableHeaderBackgroundColor");
    QColor hdrFg = ConfigColor("TableHeaderTextColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: none; outline: none; }"
        "QTableWidget::item { padding: 0 4px; }"
        "QTableWidget::item:selected { background-color: %4; }"
        "QTableWidget::item:alternate { background-color: %5; }"
    ).arg(bg.name(), fg.name(), grid.name(), sel.name(), alt.name()));
}

void CPUStack::refresh()
{
    auto entries = m_debugCore->getStackEntries(24);
    setRowCount(entries.size());

    QColor addrColor = ConfigColor("StackAddressColor");
    QColor valueColor = ConfigColor("StackTextColor");
    QColor spColor = ConfigColor("StackCurrentSPColor");
    QColor commentColor = ConfigColor("DisassemblyCommentColor");

    for (int i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];

        auto* addrItem = new QTableWidgetItem(
            QString("0x%1").arg(entry.address, 16, 16, QChar('0'))
        );
        addrItem->setForeground(addrColor);
        if (i == 0) {
            addrItem->setBackground(spColor);
            addrItem->setForeground(QColor(0xFF, 0xFF, 0xFF));  // white on black
        }
        setItem(i, 0, addrItem);

        auto* valueItem = new QTableWidgetItem(
            QString("0x%1").arg(entry.value, 16, 16, QChar('0'))
        );
        valueItem->setForeground(valueColor);
        if (i == 0) {
            valueItem->setBackground(spColor);
            valueItem->setForeground(QColor(0xFF, 0xFF, 0xFF));
        }
        setItem(i, 1, valueItem);

        auto* commentItem = new QTableWidgetItem(entry.comment);
        commentItem->setForeground(commentColor);
        if (i == 0) {
            commentItem->setBackground(spColor);
            commentItem->setForeground(QColor(0xFF, 0xFF, 0xFF));
        }
        setItem(i, 2, commentItem);
    }
}
