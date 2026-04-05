#include "MemoryMapView.h"
#include "common/Configuration.h"

#include <QHeaderView>

MemoryMapView::MemoryMapView(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    refresh();
}

void MemoryMapView::setupColumns()
{
    setColumnCount(4);
    setHorizontalHeaderLabels({"Base Address", "Size", "Name", "Permissions"});
    horizontalHeader()->setStretchLastSection(true);
    for (int i = 0; i < 3; i++)
        horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setShowGrid(false);
    setAlternatingRowColors(true);
}

void MemoryMapView::applyStyle()
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

void MemoryMapView::refresh()
{
    auto regions = m_debugCore->getMemoryMap();
    setRowCount(regions.size());

    QColor addrColor = ConfigColor("DisassemblyAddressColor");

    for (int i = 0; i < regions.size(); i++) {
        const auto& r = regions[i];

        auto* addrItem = new QTableWidgetItem(QString("0x%1").arg(r.base, 16, 16, QChar('0')));
        addrItem->setForeground(addrColor);
        setItem(i, 0, addrItem);

        setItem(i, 1, new QTableWidgetItem(QString("0x%1").arg(r.size, 0, 16)));
        setItem(i, 2, new QTableWidgetItem(r.name));
        setItem(i, 3, new QTableWidgetItem(r.permissions));
    }
}
