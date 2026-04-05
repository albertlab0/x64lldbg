#include "ThreadsView.h"
#include "common/Configuration.h"

#include <QHeaderView>

ThreadsView::ThreadsView(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    refresh();
}

void ThreadsView::setupColumns()
{
    setColumnCount(4);
    setHorizontalHeaderLabels({"ID", "PC", "Name", "Status"});
    horizontalHeader()->setStretchLastSection(true);
    for (int i = 0; i < 3; i++)
        horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setShowGrid(false);
}

void ThreadsView::applyStyle()
{
    QFont font = ConfigFont("Default");
    setFont(font);

    QColor bg = ConfigColor("DisassemblyBackgroundColor");
    QColor fg = ConfigColor("DisassemblyTextColor");
    QColor grid = ConfigColor("TableGridColor");
    QColor hdrBg = ConfigColor("TableHeaderBackgroundColor");
    QColor hdrFg = ConfigColor("TableHeaderTextColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; gridline-color: %3; }"
        "QHeaderView::section { background-color: %4; color: %5; border: 1px solid %3; padding: 2px; }"
    ).arg(bg.name(), fg.name(), grid.name(), hdrBg.name(), hdrFg.name()));
}

void ThreadsView::refresh()
{
    auto threads = m_debugCore->getThreads();
    setRowCount(threads.size());

    QColor addrColor = ConfigColor("DisassemblyAddressColor");

    for (int i = 0; i < threads.size(); i++) {
        const auto& t = threads[i];
        setItem(i, 0, new QTableWidgetItem(QString::number(t.id)));

        auto* pcItem = new QTableWidgetItem(QString("0x%1").arg(t.pc, 16, 16, QChar('0')));
        pcItem->setForeground(addrColor);
        setItem(i, 1, pcItem);

        setItem(i, 2, new QTableWidgetItem(t.name));
        setItem(i, 3, new QTableWidgetItem(t.isCurrent ? "Current" : "Suspended"));
    }
}
