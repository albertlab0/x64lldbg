#include "CPUDump.h"
#include "common/Configuration.h"

#include <QHeaderView>
#include <QVBoxLayout>
#include <QPainter>
#include <QStackedWidget>
#include <QScrollBar>

// ============================================================
//  CPUDumpView — single hex dump table
// ============================================================

CPUDumpView::CPUDumpView(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    // Start empty — hide header and columns until debugger provides an address
    horizontalHeader()->setVisible(false);
    setColumnCount(0);
}

void CPUDumpView::setupColumns()
{
    // Columns: Address | 16 hex bytes (grouped as pairs in display) | ASCII
    // We use 3 columns: Address, Hex (one wide column), ASCII
    // But to get x64dbg-style layout with individual byte selection,
    // we use: Address + 16 hex byte cols + ASCII = 18 columns
    setColumnCount(18);

    // No column header (x64dbg-style)
    horizontalHeader()->setVisible(false);
    horizontalHeader()->setMinimumSectionSize(10);

    // Address column
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    // Hex byte columns: fixed width
    for (int i = 1; i <= 16; i++) {
        horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
        setColumnWidth(i, 22);
    }

    // ASCII column stretches
    horizontalHeader()->setStretchLastSection(true);

    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setShowGrid(false);
}

void CPUDumpView::applyStyle()
{
    QFont font = ConfigFont("HexDump");
    setFont(font);

    QColor bg = ConfigColor("HexDumpBackgroundColor");
    QColor fg = ConfigColor("HexDumpTextColor");
    QColor sel = ConfigColor("DisassemblySelectionColor");
    QColor hdrBg = ConfigColor("TableHeaderBackgroundColor");
    QColor hdrFg = ConfigColor("TableHeaderTextColor");
    QColor border = ConfigColor("ChromeBorderColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: none; outline: none; }"
        "QTableWidget::item { padding: 0 1px; }"
        "QTableWidget::item:selected { background-color: %3; }"
        "QHeaderView::section { background-color: %4; color: %5; border: none;"
        "  border-right: 1px solid %6; border-bottom: 1px solid %6;"
        "  padding: 2px 4px; font-size: 12px; font-weight: 500; }"
    ).arg(bg.name(), fg.name(), sel.name(), hdrBg.name(), hdrFg.name(), border.name()));
}

void CPUDumpView::populate()
{
    if (m_baseAddress == 0) {
        setRowCount(0);
        horizontalHeader()->setVisible(false);
        return;
    }

    // Restore columns if this is the first populate
    if (columnCount() == 0)
        setupColumns();

    int rows = 20;
    setRowCount(rows);

    QColor addrColor = ConfigColor("HexDumpAddressColor");
    QColor hexColor = ConfigColor("HexDumpTextColor");
    QColor asciiColor = ConfigColor("HexDumpAsciiColor");

    QByteArray data = m_debugCore->readMemory(m_baseAddress, rows * 16);
    bool hasData = !data.isEmpty();

    for (int row = 0; row < rows; row++) {
        uint64_t addr = m_baseAddress + row * 16;

        auto* addrItem = new QTableWidgetItem(
            QString("%1").arg(addr, 16, 16, QChar('0')).toUpper()
        );
        addrItem->setForeground(addrColor);
        setItem(row, 0, addrItem);

        QString ascii;
        for (int col = 0; col < 16; col++) {
            int idx = row * 16 + col;

            QString hexText;
            if (hasData && idx < data.size()) {
                uint8_t byte = static_cast<uint8_t>(data[idx]);
                hexText = QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
                ascii += (byte >= 0x20 && byte < 0x7F) ? QChar(byte) : QChar('.');
            } else {
                hexText = "??";
                ascii += '?';
            }

            auto* hexItem = new QTableWidgetItem(hexText);
            hexItem->setForeground(hexColor);
            hexItem->setTextAlignment(Qt::AlignCenter);
            setItem(row, col + 1, hexItem);
        }

        auto* asciiItem = new QTableWidgetItem(ascii);
        asciiItem->setForeground(asciiColor);
        setItem(row, 17, asciiItem);
    }
}

void CPUDumpView::paintEvent(QPaintEvent* event)
{
    // Draw the table first
    QTableWidget::paintEvent(event);

    if (m_baseAddress == 0) return;  // nothing to decorate when empty

    // Draw vertical separator lines every 4 bytes (like x64dbg)
    QPainter painter(viewport());
    QColor lineColor = ConfigColor("TableGridColor");
    painter.setPen(QPen(lineColor, 1));

    // Draw separators after columns 4, 8, 12 (i.e., after hex cols 4, 8, 12)
    // Hex columns are 1-16, so separators after col 4, 8, 12
    for (int sepAfterCol : {4, 8, 12}) {
        int x = columnViewportPosition(sepAfterCol + 1);  // left edge of next column
        if (x > 0) {
            painter.drawLine(x - 1, 0, x - 1, viewport()->height());
        }
    }
}

void CPUDumpView::refresh()
{
    populate();
}

void CPUDumpView::goToAddress(uint64_t address)
{
    m_baseAddress = address & ~0xF; // Align to 16
    refresh();
}

// ============================================================
//  CPUDump — tabbed container with Dump 1-5
// ============================================================

CPUDump::CPUDump(DebugCore* debugCore, QWidget* parent)
    : QWidget(parent)
    , m_debugCore(debugCore)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Tab bar (like x64dbg's Dump 1..5 tabs)
    m_tabBar = new QTabBar(this);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);

    auto* stack = new QStackedWidget(this);

    for (int i = 0; i < 5; i++) {
        m_dumpViews[i] = new CPUDumpView(debugCore, this);
        m_tabBar->addTab(QString("Dump %1").arg(i + 1));
        stack->addWidget(m_dumpViews[i]);
    }

    m_tabBar->setCurrentIndex(0);
    connect(m_tabBar, &QTabBar::currentChanged, this, [this, stack](int index) {
        m_currentTab = index;
        stack->setCurrentIndex(index);
    });

    // Style the tab bar to match theme
    QColor chrome = ConfigColor("ChromeBackgroundColor");
    QColor surface = ConfigColor("ChromeSurfaceColor");
    QColor fg = ConfigColor("HexDumpTextColor");
    QColor muted = ConfigColor("ChromeMutedTextColor");
    QColor accent = ConfigColor("ChromeAccentColor");

    m_tabBar->setStyleSheet(QString(
        "QTabBar { background: %1; }"
        "QTabBar::tab { background: %1; color: %4; padding: 4px 16px; min-width: 50px;"
        "  border: none; border-bottom: 2px solid transparent; font-size: 12px; }"
        "QTabBar::tab:selected { color: %3; border-bottom: 2px solid %5; background: %2; }"
        "QTabBar::tab:hover:!selected { color: %3; }"
    ).arg(chrome.name(), surface.name(), fg.name(), muted.name(), accent.name()));

    layout->addWidget(m_tabBar, 0, Qt::AlignLeft);
    layout->addWidget(stack);
}

CPUDumpView* CPUDump::currentDumpView() const
{
    return m_dumpViews[m_currentTab];
}

void CPUDump::refresh()
{
    // Refresh the active dump view
    m_dumpViews[m_currentTab]->refresh();
}

void CPUDump::goToAddress(uint64_t address)
{
    m_dumpViews[m_currentTab]->goToAddress(address);
}
