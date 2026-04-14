#include "CPUDisassembly.h"
#include "common/Configuration.h"
#include "gui/dialogs/GotoDialog.h"

#include <QHeaderView>
#include <QSet>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QScrollBar>

CPUDisassembly::CPUDisassembly(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    setupContextMenu();

    connect(this, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row >= 0 && row < m_lines.size()) {
            // Clear goto highlight when user clicks a different row
            if (m_gotoAddress != 0 && m_lines[row].address != m_gotoAddress) {
                uint64_t oldGoto = m_gotoAddress;
                m_gotoAddress = 0;
                // Repaint the old goto row to remove highlight
                QColor defaultBg = ConfigColor("DisassemblyBackgroundColor");
                for (int i = 0; i < m_lines.size(); i++) {
                    if (m_lines[i].address == oldGoto) {
                        QColor mnemBg = bgColorForMnemonic(m_lines[i].mnemonic);
                        QColor bg = mnemBg.isValid() ? mnemBg : defaultBg;
                        for (int col = 0; col < 4; col++) {
                            if (auto* it = item(i, col))
                                it->setBackground(bg);
                        }
                        break;
                    }
                }
            }
            emit addressSelected(m_lines[row].address);
        }
    });
}

void CPUDisassembly::setupColumns()
{
    setColumnCount(4);  // Address | Bytes | Disassembly | Comments
    horizontalHeader()->setVisible(false);
    horizontalHeader()->setStretchLastSection(true);
    // Interactive resize — user can drag column borders like x64dbg
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    horizontalHeader()->setMinimumSectionSize(30);
    // Default column widths
    setColumnWidth(0, 140);  // Address
    setColumnWidth(1, 120);  // Bytes
    setColumnWidth(2, 300);  // Disassembly (mnemonic + operands)
    // Column 3 (comments) stretches to fill
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    setAlternatingRowColors(false);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void CPUDisassembly::applyStyle()
{
    QFont font = ConfigFont("Disassembly");
    setFont(font);

    QColor bg = ConfigColor("DisassemblyBackgroundColor");
    QColor fg = ConfigColor("DisassemblyTextColor");
    QColor sel = ConfigColor("DisassemblySelectionColor");
    QColor grid = ConfigColor("TableGridColor");
    QColor hdrBg = ConfigColor("TableHeaderBackgroundColor");
    QColor hdrFg = ConfigColor("TableHeaderTextColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: none; outline: none; }"
        "QTableWidget::item { padding: 0 4px; }"
        "QTableWidget::item:selected { background-color: %4; }"
    ).arg(bg.name(), fg.name(), grid.name(), sel.name()));
}

void CPUDisassembly::setupContextMenu()
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);

        // Go to submenu (like x64dbg)
        auto* gotoMenu = menu.addMenu("Go to");

        auto* gotoExpr = gotoMenu->addAction("Expression...");
        gotoExpr->setShortcut(Config()->getShortcut("GotoAddress").hotkey);
        connect(gotoExpr, &QAction::triggered, this, [this]() {
            GotoDialog dlg(m_debugCore, this);
            if (dlg.exec() == QDialog::Accepted)
                goToAddress(dlg.resultAddress());
        });

        auto* gotoOrigin = gotoMenu->addAction("Origin (RIP)");
        gotoOrigin->setShortcut(Config()->getShortcut("GotoOrigin").hotkey);
        connect(gotoOrigin, &QAction::triggered, this, [this]() {
            uint64_t pc = m_debugCore->currentPC();
            if (pc != 0)
                goToAddress(pc);
        });

        // Toggle breakpoint
        menu.addSeparator();
        auto* toggleBP = menu.addAction("Toggle Breakpoint");
        toggleBP->setShortcut(Config()->getShortcut("ToggleBreakpoint").hotkey);
        connect(toggleBP, &QAction::triggered, this, [this]() {
            uint64_t addr = selectedAddress();
            if (addr != 0)
                m_debugCore->toggleBreakpoint(addr);
        });

        menu.exec(viewport()->mapToGlobal(pos));
    });

    // Disassembly-local shortcuts — use WidgetShortcut to avoid
    // ambiguity with MainWindow menu actions that share the same keys.
    auto addLocalAction = [this](const QString& shortcutId, auto slot) {
        auto* action = new QAction(this);
        action->setShortcut(Config()->getShortcut(shortcutId).hotkey);
        action->setShortcutContext(Qt::WidgetShortcut);
        connect(action, &QAction::triggered, this, slot);
        addAction(action);
    };

    // Ctrl+G — Go to Address
    addLocalAction("GotoAddress", [this]() {
        GotoDialog dlg(m_debugCore, this);
        if (dlg.exec() == QDialog::Accepted)
            goToAddress(dlg.resultAddress());
    });

    // * — Go to Origin (current RIP)
    addLocalAction("GotoOrigin", [this]() {
        uint64_t pc = m_debugCore->currentPC();
        if (pc != 0)
            goToAddress(pc);
    });

    // F2 — Toggle Breakpoint
    addLocalAction("ToggleBreakpoint", [this]() {
        uint64_t addr = selectedAddress();
        if (addr != 0)
            m_debugCore->toggleBreakpoint(addr);
    });

    // F4 — Run to Cursor
    addLocalAction("DebugRunToCursor", [this]() {
        uint64_t addr = selectedAddress();
        if (addr != 0)
            m_debugCore->runToCursor(addr);
    });
}

QColor CPUDisassembly::colorForMnemonic(const QString& mnemonic) const
{
    if (mnemonic == "call" || mnemonic == "callq")
        return ConfigColor("DisassemblyCallColor");
    if (mnemonic == "ret" || mnemonic == "retq" || mnemonic == "leave")
        return ConfigColor("DisassemblyRetColor");
    if (mnemonic == "nop" || mnemonic == "nopw" || mnemonic == "nopl")
        return ConfigColor("DisassemblyNopColor");
    // Unconditional jump: black text (x64dbg: InstructionUnconditionalJumpColor = #000000)
    if (mnemonic == "jmp" || mnemonic == "jmpq")
        return ConfigColor("DisassemblyMnemonicColor");
    // Conditional jumps: red text
    if (mnemonic.startsWith('j'))
        return ConfigColor("DisassemblyConditionalJumpColor");
    if (mnemonic == "push" || mnemonic == "pushq" || mnemonic == "pop" || mnemonic == "popq")
        return ConfigColor("DisassemblyPushPopColor");
    return ConfigColor("DisassemblyMnemonicColor");
}

// x64dbg highlights entire rows for calls/rets/jumps with background colors
QColor CPUDisassembly::bgColorForMnemonic(const QString& mnemonic) const
{
    if (mnemonic == "call" || mnemonic == "callq")
        return ConfigColor("DisassemblyCallBgColor");
    if (mnemonic == "ret" || mnemonic == "retq" || mnemonic == "leave")
        return ConfigColor("DisassemblyRetBgColor");
    // Both unconditional and conditional jumps get yellow background
    if (mnemonic.startsWith('j'))
        return ConfigColor("DisassemblyConditionalJumpBgColor");
    return QColor();  // invalid = no special bg
}

void CPUDisassembly::refresh()
{
    // Clear user navigation highlight on debug events (stepping, breakpoint hit, etc.)
    m_gotoAddress = 0;

    uint64_t pc = m_debugCore->currentPC();
    if (pc == 0) return;

    // x64dbg-style lazy scrolling:
    // Check if the new IP is within the currently cached viewport.
    // If yes, just repaint highlights — no re-disassembly, no scrolling.
    // If no, re-disassemble starting from the new IP.
    if (!m_lines.isEmpty() && m_baseAddress == 0) {
        bool ipInViewport = false;
        for (const auto& line : m_lines) {
            if (line.address == pc) {
                ipInViewport = true;
                break;
            }
        }

        if (ipInViewport) {
            // IP is still visible — just update highlights, don't scroll
            updateHighlights(pc);
            return;
        }
    }

    // IP is off-screen (or first load) — full re-disassemble
    populateFromAddress(pc);
}

void CPUDisassembly::goToAddress(uint64_t address)
{
    m_gotoAddress = address;
    m_baseAddress = address;
    populateFromAddress(address);
}

void CPUDisassembly::populateFromAddress(uint64_t address)
{
    m_lines = m_debugCore->disassemble(address, 128);
    if (m_lines.isEmpty()) return;

    m_baseAddress = 0;

    rebuildTable();
    emit linesChanged();
}

void CPUDisassembly::rebuildTable()
{
    uint64_t pc = m_debugCore->currentPC();

    setRowCount(m_lines.size());

    QColor addrColor = ConfigColor("DisassemblyAddressColor");
    QColor bytesColor = ConfigColor("DisassemblyBytesColor");
    QColor operColor = ConfigColor("DisassemblyTextColor");
    QColor ipBg = ConfigColor("CurrentIPBackgroundColor");
    QColor bpBg = ConfigColor("BreakpointBackgroundColor");
    QColor commentColor = ConfigColor("DisassemblyCommentColor");

    // Get breakpoint addresses for highlighting
    QSet<uint64_t> bpAddresses;
    auto bps = m_debugCore->getBreakpoints();
    for (const auto& bp : bps) {
        if (bp.enabled)
            bpAddresses.insert(bp.address);
    }

    for (int i = 0; i < m_lines.size(); i++) {
        const auto& line = m_lines[i];

        // Address
        auto* addrItem = new QTableWidgetItem(
            QString("%1").arg(line.address, 16, 16, QChar('0')).toUpper()
        );
        addrItem->setForeground(addrColor);
        setItem(i, 0, addrItem);

        // Bytes as hex string
        QString bytesStr;
        for (int b = 0; b < line.bytes.size(); b++) {
            if (b > 0) bytesStr += ' ';
            bytesStr += QString("%1").arg(static_cast<uint8_t>(line.bytes[b]), 2, 16, QChar('0'));
        }
        auto* bytesItem = new QTableWidgetItem(bytesStr.toUpper());
        bytesItem->setForeground(bytesColor);
        setItem(i, 1, bytesItem);

        // Disassembly (mnemonic + operands in one column)
        QString disasmText = line.mnemonic;
        if (!line.operands.isEmpty())
            disasmText += " " + line.operands;
        auto* disasmItem = new QTableWidgetItem(disasmText);
        disasmItem->setForeground(colorForMnemonic(line.mnemonic));
        setItem(i, 2, disasmItem);

        // Comments
        auto* commentItem = new QTableWidgetItem(
            line.comment.isEmpty() ? QString() : "; " + line.comment);
        commentItem->setForeground(commentColor);
        setItem(i, 3, commentItem);

        // Highlight current IP, breakpoints, goto target, and mnemonic-based row colors
        bool isIP = (line.address == pc);
        bool isBP = bpAddresses.contains(line.address);
        bool isGoto = (line.address == m_gotoAddress && m_gotoAddress != 0);

        QColor rowBg;
        if (isIP) {
            rowBg = ipBg;
            // CIP: white text on black background
            QColor cipText = ConfigColor("CurrentIPColor");
            for (int col = 0; col < 4; col++) {
                if (auto* it = item(i, col)) {
                    it->setBackground(rowBg);
                    it->setForeground(cipText);
                }
            }
        } else if (isBP) {
            // x64dbg style: only the address column is highlighted red
            if (auto* it = item(i, 0)) {
                it->setBackground(bpBg);
                it->setForeground(ConfigColor("CurrentIPColor")); // white text on red
            }
        } else if (isGoto) {
            // User navigation highlight (Ctrl+G) — uses selection color from theme
            QColor gotoBg = ConfigColor("DisassemblySelectionColor");
            for (int col = 0; col < 4; col++) {
                if (auto* it = item(i, col))
                    it->setBackground(gotoBg);
            }
        } else {
            // x64dbg-style: calls/rets/jumps get colored row backgrounds
            QColor mnemBg = bgColorForMnemonic(line.mnemonic);
            if (mnemBg.isValid()) {
                for (int col = 0; col < 4; col++) {
                    if (auto* it = item(i, col))
                        it->setBackground(mnemBg);
                }
            }
        }

        // Scroll to goto target or IP row
        if (isGoto) {
            blockSignals(true);
            clearSelection();
            setCurrentCell(i, 0);
            scrollToItem(item(i, 0), QAbstractItemView::PositionAtCenter);
            blockSignals(false);
            emit addressSelected(line.address);
        } else if (isIP && m_gotoAddress == 0) {
            blockSignals(true);
            setCurrentCell(i, 0);
            scrollToItem(item(i, 0), QAbstractItemView::PositionAtCenter);
            blockSignals(false);
            emit addressSelected(line.address);
        }
    }
}

void CPUDisassembly::updateHighlights(uint64_t pc)
{
    // Fast path: IP is in viewport, just update row backgrounds.
    // No re-disassembly, no scrolling.

    QColor ipBg = ConfigColor("CurrentIPBackgroundColor");
    QColor bpBg = ConfigColor("BreakpointBackgroundColor");
    QColor defaultBg = ConfigColor("DisassemblyBackgroundColor");

    QSet<uint64_t> bpAddresses;
    auto bps = m_debugCore->getBreakpoints();
    for (const auto& bp : bps) {
        if (bp.enabled)
            bpAddresses.insert(bp.address);
    }

    QColor cipText = ConfigColor("CurrentIPColor");
    QColor addrColor = ConfigColor("DisassemblyAddressColor");
    QColor bytesColor = ConfigColor("DisassemblyBytesColor");
    QColor operColor = ConfigColor("DisassemblyTextColor");
    QColor commentColor = ConfigColor("DisassemblyCommentColor");

    for (int i = 0; i < m_lines.size(); i++) {
        bool isIP = (m_lines[i].address == pc);
        bool isBP = bpAddresses.contains(m_lines[i].address);
        const auto& line = m_lines[i];

        if (isIP) {
            for (int col = 0; col < 4; col++) {
                if (auto* it = item(i, col)) {
                    it->setBackground(ipBg);
                    it->setForeground(cipText);
                }
            }
        } else if (isBP) {
            // x64dbg style: only address column highlighted red
            for (int col = 0; col < 4; col++) {
                if (auto* it = item(i, col)) {
                    if (col == 0) {
                        it->setBackground(bpBg);
                        it->setForeground(cipText); // white on red
                    } else {
                        QColor mnemBg = bgColorForMnemonic(line.mnemonic);
                        it->setBackground(mnemBg.isValid() ? mnemBg : defaultBg);
                        switch (col) {
                        case 1: it->setForeground(bytesColor); break;
                        case 2: it->setForeground(colorForMnemonic(line.mnemonic)); break;
                        case 3: it->setForeground(commentColor); break;
                        }
                    }
                }
            }
        } else {
            QColor mnemBg = bgColorForMnemonic(line.mnemonic);
            QColor bg = mnemBg.isValid() ? mnemBg : defaultBg;
            for (int col = 0; col < 4; col++) {
                if (auto* it = item(i, col)) {
                    it->setBackground(bg);
                    switch (col) {
                    case 0: it->setForeground(addrColor); break;
                    case 1: it->setForeground(bytesColor); break;
                    case 2: it->setForeground(colorForMnemonic(line.mnemonic)); break;
                    case 3: it->setForeground(commentColor); break;
                    }
                }
            }
        }
    }

    // Don't scroll — just move the selection highlight to the IP row
    // Use blockSignals to avoid triggering addressSelected during internal update
    blockSignals(true);
    for (int i = 0; i < m_lines.size(); i++) {
        if (m_lines[i].address == pc) {
            setCurrentCell(i, 0);
            break;
        }
    }
    blockSignals(false);

    // Emit addressSelected for the InfoBox
    emit addressSelected(pc);
}

void CPUDisassembly::paintEvent(QPaintEvent* event)
{
    // Let QTableWidget paint everything first
    QTableWidget::paintEvent(event);

    // Draw vertical separator lines between columns (x64dbg style)
    QPainter painter(viewport());
    QColor sepColor = ConfigColor("TableGridColor");
    painter.setPen(QPen(sepColor, 1));

    int viewHeight = viewport()->height();
    int xOffset = -horizontalScrollBar()->value();

    // Draw vertical separators: Address | Bytes | Disassembly | Comments
    for (int col = 0; col < columnCount() - 1; col++) {
        xOffset += columnWidth(col);
        painter.drawLine(xOffset - 1, 0, xOffset - 1, viewHeight);
    }
}

uint64_t CPUDisassembly::selectedAddress() const
{
    int row = currentRow();
    if (row >= 0 && row < m_lines.size())
        return m_lines[row].address;
    return 0;
}
