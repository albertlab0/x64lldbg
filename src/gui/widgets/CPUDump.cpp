#include "CPUDump.h"
#include "common/Configuration.h"

#include <QHeaderView>
#include <QVBoxLayout>
#include <QPainter>
#include <QStackedWidget>
#include <QScrollBar>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QRegularExpression>

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

    // Double-click a hex byte cell to edit
    connect(this, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col >= 1 && col <= 16)
            editByteAt(row, col);
    });

    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (m_baseAddress == 0) return;
        QMenu menu(this);
        auto* editAction = menu.addAction("Edit Bytes...");
        editAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
        connect(editAction, &QAction::triggered, this, &CPUDumpView::editBytesDialog);
        menu.exec(viewport()->mapToGlobal(pos));
    });
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

    // Address column — fixed width for 16 hex chars
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    setColumnWidth(0, 150);

    // Hex byte columns: fixed width (28px to accommodate macOS font rendering)
    for (int i = 1; i <= 16; i++) {
        horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
        setColumnWidth(i, 28);
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

    int totalBytes = rows * 16;
    QByteArray data = m_debugCore->readMemory(m_baseAddress, totalBytes);

    // Ensure data is exactly the right size — pad with zeros if short
    if (data.size() < totalBytes)
        data.append(QByteArray(totalBytes - data.size(), '\0'));

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
            uint8_t byte = static_cast<uint8_t>(data[idx]);

            auto* hexItem = new QTableWidgetItem(
                QString("%1").arg(byte, 2, 16, QChar('0')).toUpper()
            );
            hexItem->setForeground(hexColor);
            hexItem->setTextAlignment(Qt::AlignCenter);
            setItem(row, col + 1, hexItem);

            ascii += (byte >= 0x20 && byte < 0x7F) ? QChar(byte) : QChar('.');
        }

        auto* asciiItem = new QTableWidgetItem(ascii);
        asciiItem->setForeground(asciiColor);
        setItem(row, 17, asciiItem);
    }
}

void CPUDumpView::editByteAt(int row, int col)
{
    if (m_baseAddress == 0 || col < 1 || col > 16) return;

    uint64_t addr = m_baseAddress + row * 16 + (col - 1);
    auto* hexItem = item(row, col);
    if (!hexItem) return;

    bool ok;
    QString newVal = QInputDialog::getText(this,
        QString("Edit Byte at 0x%1").arg(addr, 0, 16),
        "Hex value (1 byte):",
        QLineEdit::Normal, hexItem->text(), &ok);

    if (!ok || newVal.trimmed().isEmpty()) return;

    QString stripped = newVal.trimmed();
    if (stripped.startsWith("0x", Qt::CaseInsensitive))
        stripped = stripped.mid(2);

    bool parseOk;
    uint val = stripped.toUInt(&parseOk, 16);
    if (!parseOk || val > 0xFF) return;

    QByteArray data;
    data.append(static_cast<char>(val));
    if (m_debugCore->writeMemory(addr, data))
        refresh();
}

void CPUDumpView::editBytesDialog()
{
    if (m_baseAddress == 0) return;

    // Use selected cell address if available, else base address
    uint64_t addr = m_baseAddress;
    auto sel = selectedItems();
    if (!sel.isEmpty()) {
        int row = sel.first()->row();
        int col = sel.first()->column();
        if (col >= 1 && col <= 16)
            addr = m_baseAddress + row * 16 + (col - 1);
        else
            addr = m_baseAddress + row * 16;
    }

    bool ok;
    QString addrStr = QInputDialog::getText(this,
        "Edit Memory",
        "Address (hex):",
        QLineEdit::Normal, QString("0x%1").arg(addr, 0, 16), &ok);
    if (!ok || addrStr.trimmed().isEmpty()) return;

    QString stripped = addrStr.trimmed();
    if (stripped.startsWith("0x", Qt::CaseInsensitive))
        stripped = stripped.mid(2);
    uint64_t targetAddr = stripped.toULongLong(&ok, 16);
    if (!ok) return;

    QString bytesStr = QInputDialog::getText(this,
        QString("Edit Memory at 0x%1").arg(targetAddr, 0, 16),
        "Hex bytes (e.g., 90 90 CC 48 89):",
        QLineEdit::Normal, QString(), &ok);
    if (!ok || bytesStr.trimmed().isEmpty()) return;

    // Parse space-separated hex bytes
    QByteArray data;
    QStringList tokens = bytesStr.trimmed().split(QRegularExpression("\\s+"));
    for (const QString& token : tokens) {
        bool byteOk;
        uint byte = token.toUInt(&byteOk, 16);
        if (!byteOk || byte > 0xFF) return;
        data.append(static_cast<char>(byte));
    }

    if (!data.isEmpty() && m_debugCore->writeMemory(targetAddr, data))
        refresh();
}

void CPUDumpView::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_E) {
        editBytesDialog();
        return;
    }
    QTableWidget::keyPressEvent(event);
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

        // Auto-populate empty dump tabs with Dump 1's address
        if (m_dumpViews[index]->baseAddress() == 0) {
            uint64_t addr = m_dumpViews[0]->baseAddress();
            if (addr != 0)
                m_dumpViews[index]->goToAddress(addr);
        }
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
