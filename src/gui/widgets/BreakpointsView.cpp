#include "BreakpointsView.h"
#include "common/Configuration.h"

#include <QHeaderView>

BreakpointsView::BreakpointsView(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();

    connect(this, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        // Address is in column 1, formatted as "0x..."
        if (auto* addrItem = item(row, 1)) {
            bool ok;
            uint64_t addr = addrItem->text().toULongLong(&ok, 16);
            if (ok)
                emit breakpointDoubleClicked(addr);
        }
    });
}

void BreakpointsView::setupColumns()
{
    setColumnCount(6);
    setHorizontalHeaderLabels({"Type", "Address", "Module", "Status", "Hit Count", "Condition"});
    horizontalHeader()->setStretchLastSection(true);
    for (int i = 0; i < 5; i++)
        horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setShowGrid(false);
    setAlternatingRowColors(true);
}

void BreakpointsView::applyStyle()
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

void BreakpointsView::refresh()
{
    auto breakpoints = m_debugCore->getBreakpoints();
    setRowCount(breakpoints.size());

    QColor addrColor = ConfigColor("DisassemblyAddressColor");
    QColor enabledColor = ConfigColor("BreakpointColor");
    QColor disabledColor = ConfigColor("DisassemblyCommentColor");

    for (int i = 0; i < breakpoints.size(); i++) {
        const auto& bp = breakpoints[i];

        setItem(i, 0, new QTableWidgetItem(bp.isHardware ? "Hardware" : "Software"));

        auto* addrItem = new QTableWidgetItem(
            QString("0x%1").arg(bp.address, 16, 16, QChar('0')));
        addrItem->setForeground(addrColor);
        setItem(i, 1, addrItem);

        setItem(i, 2, new QTableWidgetItem(bp.module));

        auto* statusItem = new QTableWidgetItem(bp.enabled ? "Enabled" : "Disabled");
        statusItem->setForeground(bp.enabled ? enabledColor : disabledColor);
        setItem(i, 3, statusItem);

        setItem(i, 4, new QTableWidgetItem(QString::number(bp.hitCount)));
        setItem(i, 5, new QTableWidgetItem(bp.condition));
    }
}
