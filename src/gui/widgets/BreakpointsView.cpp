#include "BreakpointsView.h"
#include "common/Configuration.h"
#include "gui/dialogs/EditBreakpointDialog.h"

#include <QHeaderView>
#include <QMenu>

BreakpointsView::BreakpointsView(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    setupContextMenu();

    connect(this, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        editBreakpointAt(row);
    });
}

void BreakpointsView::setupColumns()
{
    setColumnCount(7);
    setHorizontalHeaderLabels({"Type", "Size", "Address", "Module", "Status", "Hit Count", "Condition"});
    horizontalHeader()->setStretchLastSection(true);
    for (int i = 0; i < 6; i++)
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

void BreakpointsView::setupContextMenu()
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        int row = rowAt(pos.y());
        if (row < 0) return;
        auto bps = m_debugCore->getBreakpoints();
        if (row >= bps.size()) return;
        BreakpointInfo bp = bps[row];

        QMenu menu(this);

        auto* editAction = menu.addAction("Edit Breakpoint...");
        bool isWatch = (bp.kind == BreakpointKind::WatchWrite ||
                        bp.kind == BreakpointKind::WatchReadWrite);
        editAction->setEnabled(!isWatch);
        connect(editAction, &QAction::triggered, this, [this, row]() {
            editBreakpointAt(row);
        });

        menu.addSeparator();

        auto* deleteAction = menu.addAction("Delete Breakpoint");
        deleteAction->setShortcut(Qt::Key_Delete);
        connect(deleteAction, &QAction::triggered, this, [this, bp]() {
            m_debugCore->removeBreakpoint(bp);
        });

        auto* toggleAction = menu.addAction("Enable/Disable");
        toggleAction->setShortcut(Qt::Key_Space);
        connect(toggleAction, &QAction::triggered, this, [this, bp]() {
            m_debugCore->toggleBreakpointEnabled(bp);
        });

        menu.addSeparator();

        auto* gotoAction = menu.addAction("Go to in Disassembly");
        connect(gotoAction, &QAction::triggered, this, [this, bp]() {
            emit breakpointDoubleClicked(bp.address);
        });

        menu.exec(viewport()->mapToGlobal(pos));
    });
}

void BreakpointsView::editBreakpointAt(int row)
{
    auto breakpoints = m_debugCore->getBreakpoints();
    if (row < 0 || row >= breakpoints.size()) return;

    const auto& bp = breakpoints[row];
    if (bp.kind == BreakpointKind::WatchWrite ||
        bp.kind == BreakpointKind::WatchReadWrite)
        return;  // watchpoint editing not supported yet

    EditBreakpointDialog dlg(m_debugCore, bp, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_debugCore->setBreakpointCondition(bp.id, dlg.breakCondition());
        m_debugCore->setBreakpointLogText(bp.id, dlg.logText());
        m_debugCore->setBreakpointLogCondition(bp.id, dlg.logCondition());
        m_debugCore->setBreakpointFastResume(bp.id, dlg.fastResume());
        m_debugCore->setBreakpointDump(bp.id, dlg.dumpAddress(),
                                        dlg.dumpSize(), dlg.dumpFilename());
    }
}

void BreakpointsView::refresh()
{
    auto breakpoints = m_debugCore->getBreakpoints();
    setRowCount(breakpoints.size());

    QColor addrColor = ConfigColor("DisassemblyAddressColor");
    QColor enabledColor = ConfigColor("BreakpointColor");
    QColor hwColor = ConfigColor("HardwareBreakpointColor");
    QColor disabledColor = ConfigColor("DisassemblyCommentColor");

    for (int i = 0; i < breakpoints.size(); i++) {
        const auto& bp = breakpoints[i];

        QString typeStr;
        switch (bp.kind) {
        case BreakpointKind::Software:       typeStr = "Software";       break;
        case BreakpointKind::HardwareExec:   typeStr = "Hardware (Exec)"; break;
        case BreakpointKind::WatchWrite:     typeStr = "Watch (Write)";  break;
        case BreakpointKind::WatchReadWrite: typeStr = "Watch (R/W)";    break;
        }
        auto* typeItem = new QTableWidgetItem(typeStr);
        if (bp.isHardware) typeItem->setForeground(hwColor);
        setItem(i, 0, typeItem);

        setItem(i, 1, new QTableWidgetItem(
            bp.watchSize > 0 ? QString::number(bp.watchSize) : QString("—")));

        auto* addrItem = new QTableWidgetItem(
            QString("0x%1").arg(bp.address, 16, 16, QChar('0')));
        addrItem->setForeground(addrColor);
        setItem(i, 2, addrItem);

        setItem(i, 3, new QTableWidgetItem(bp.module));

        auto* statusItem = new QTableWidgetItem(bp.enabled ? "Enabled" : "Disabled");
        statusItem->setForeground(bp.enabled ? enabledColor : disabledColor);
        setItem(i, 4, statusItem);

        setItem(i, 5, new QTableWidgetItem(QString::number(bp.hitCount)));

        // Show condition + log summary
        QString condText = bp.condition;
        if (!bp.logText.isEmpty()) {
            if (!condText.isEmpty()) condText += " | ";
            condText += "Log: " + bp.logText;
            if (bp.fastResume)
                condText += " [FR]";
        }
        setItem(i, 6, new QTableWidgetItem(condText));
    }
}
