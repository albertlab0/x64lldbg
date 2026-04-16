#include "CPUDisassembly.h"
#include "common/Configuration.h"
#include "gui/dialogs/GotoDialog.h"

#include <QHeaderView>
#include <QSet>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QScrollBar>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QInputDialog>

CPUDisassembly::CPUDisassembly(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupColumns();
    applyStyle();
    setupContextMenu();
    setFocusPolicy(Qt::WheelFocus);

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
                        QColor bg = defaultBg;
                        for (int col = 0; col < 5; col++) {
                            if (auto* it = item(i, col))
                                it->setBackground(bg);
                        }
                        break;
                    }
                }
            }
            emit addressSelected(m_lines[row].address);
        }
        // Repaint viewport so inline flow line updates for new selection
        viewport()->update();
    });
}

void CPUDisassembly::setupColumns()
{
    setColumnCount(5);  // Address | Bytes | Mnemonic | Operands | Comments

    // No visible header — column resize is handled via custom mouse events
    // on the viewport (x64dbg-style: drag the vertical separator lines)
    horizontalHeader()->setVisible(false);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    horizontalHeader()->setMinimumSectionSize(20);
    // Default column widths
    setColumnWidth(0, 200);  // Address (wider for labels)
    setColumnWidth(1, 120);  // Bytes
    setColumnWidth(2, 60);   // Mnemonic
    setColumnWidth(3, 240);  // Operands
    // Column 4 (comments) stretches to fill
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    setAlternatingRowColors(false);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMouseTracking(true);  // needed for resize cursor on hover
}

void CPUDisassembly::applyStyle()
{
    QFont font = ConfigFont("Disassembly");
    setFont(font);

    QColor bg = ConfigColor("DisassemblyBackgroundColor");
    QColor fg = ConfigColor("DisassemblyTextColor");
    QColor sel = ConfigColor("DisassemblySelectionColor");
    QColor grid = ConfigColor("TableGridColor");

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

        // Label
        menu.addSeparator();
        auto* setLabel = menu.addAction("Label Current Address");
        setLabel->setShortcut(Config()->getShortcut("SetLabel").hotkey);
        connect(setLabel, &QAction::triggered, this, [this]() {
            promptSetLabel();
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

    // Ctrl+G is handled at MainWindow level so it works regardless of focus

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

    // : — Set Label
    addLocalAction("SetLabel", [this]() {
        promptSetLabel();
    });

    // Refresh disassembly when labels change
    connect(m_debugCore, &DebugCore::labelsChanged, this, [this]() {
        if (!m_lines.isEmpty())
            rebuildTable();
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
    // Unconditional jump
    if (mnemonic == "jmp" || mnemonic == "jmpq")
        return ConfigColor("DisassemblyUnconditionalJumpColor");
    // Conditional jumps
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
    QColor labelColor = ConfigColor("DisassemblyLabelColor");
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

        // Address — append <label> if one is set
        QString addrText = QString("%1").arg(line.address, 16, 16, QChar('0')).toUpper();
        QString addrLabel = m_debugCore->getLabel(line.address);
        if (!addrLabel.isEmpty())
            addrText += " <" + addrLabel + ">";
        auto* addrItem = new QTableWidgetItem(addrText);
        addrItem->setForeground(addrLabel.isEmpty() ? addrColor : labelColor);
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

        // Mnemonic — colored by instruction type
        auto* mnemItem = new QTableWidgetItem(line.mnemonic);
        mnemItem->setForeground(colorForMnemonic(line.mnemonic));
        setItem(i, 2, mnemItem);

        // Operands — strip 0x prefix (x64dbg defaults to bare hex)
        // For branch targets with labels, show <label> instead of raw address
        QString displayOper = line.operands;
        displayOper.replace("0x", "", Qt::CaseInsensitive);
        bool isBranch = line.mnemonic.startsWith('j') ||
                        line.mnemonic == "call" || line.mnemonic == "callq";
        if (isBranch && line.branchTarget != 0) {
            QString targetLabel = m_debugCore->getLabel(line.branchTarget);
            if (!targetLabel.isEmpty())
                displayOper = "<" + targetLabel + ">";
        }
        auto* operItem = new QTableWidgetItem(displayOper);
        operItem->setForeground(isBranch ?
            ConfigColor("DisassemblyAddressOperandColor") : operColor);
        setItem(i, 3, operItem);

        // Comments — also show label if address has one
        QString commentText;
        if (!addrLabel.isEmpty())
            commentText = "<" + addrLabel + ">";
        if (!line.comment.isEmpty()) {
            if (!commentText.isEmpty()) commentText += " ";
            commentText += "; " + line.comment;
        }
        auto* commentItem = new QTableWidgetItem(commentText);
        commentItem->setForeground(commentColor);
        setItem(i, 4, commentItem);

        // x64dbg-style instruction-type background on disassembly columns only
        // Mnemonic col gets its own bg (cyan for call, yellow for jumps)
        // Operand col always gets yellow (jump bg) for all branch types
        QColor mnBg = bgColorForMnemonic(line.mnemonic);
        if (mnBg.isValid()) {
            if (auto* it = item(i, 2)) it->setBackground(mnBg);
            QColor operBg = ConfigColor("DisassemblyConditionalJumpBgColor");
            if (auto* it = item(i, 3)) it->setBackground(operBg);
        }

        // Highlight current IP, breakpoints, goto target
        bool isIP = (line.address == pc);
        bool isBP = bpAddresses.contains(line.address);
        bool isGoto = (line.address == m_gotoAddress && m_gotoAddress != 0);

        if (isIP) {
            // CIP: white text on dark background for entire row
            QColor cipText = ConfigColor("CurrentIPColor");
            for (int col = 0; col < 5; col++) {
                if (auto* it = item(i, col)) {
                    it->setBackground(ipBg);
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
            // User navigation highlight (Ctrl+G)
            QColor gotoBg = ConfigColor("DisassemblySelectionColor");
            for (int col = 0; col < 5; col++) {
                if (auto* it = item(i, col))
                    it->setBackground(gotoBg);
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
            for (int col = 0; col < 5; col++) {
                if (auto* it = item(i, col)) {
                    it->setBackground(ipBg);
                    it->setForeground(cipText);
                }
            }
        } else if (isBP) {
            // x64dbg style: address column red, disasm columns keep instruction bg
            QColor mnBg = bgColorForMnemonic(line.mnemonic);
            QColor operBg = mnBg.isValid() ? ConfigColor("DisassemblyConditionalJumpBgColor") : QColor();
            bool isBranch = line.mnemonic.startsWith('j') ||
                            line.mnemonic == "call" || line.mnemonic == "callq";
            QColor opColor = isBranch ?
                ConfigColor("DisassemblyAddressOperandColor") : operColor;
            for (int col = 0; col < 5; col++) {
                if (auto* it = item(i, col)) {
                    if (col == 0) {
                        it->setBackground(bpBg);
                        it->setForeground(cipText); // white on red
                    } else {
                        QColor bg = defaultBg;
                        if (col == 2 && mnBg.isValid()) bg = mnBg;
                        else if (col == 3 && operBg.isValid()) bg = operBg;
                        it->setBackground(bg);
                        switch (col) {
                        case 1: it->setForeground(bytesColor); break;
                        case 2: it->setForeground(colorForMnemonic(line.mnemonic)); break;
                        case 3: it->setForeground(opColor); break;
                        case 4: it->setForeground(commentColor); break;
                        }
                    }
                }
            }
        } else {
            QColor mnBg = bgColorForMnemonic(line.mnemonic);
            QColor operBg = mnBg.isValid() ? ConfigColor("DisassemblyConditionalJumpBgColor") : QColor();
            bool isBranch = line.mnemonic.startsWith('j') ||
                            line.mnemonic == "call" || line.mnemonic == "callq";
            QColor opColor = isBranch ?
                ConfigColor("DisassemblyAddressOperandColor") : operColor;
            for (int col = 0; col < 5; col++) {
                if (auto* it = item(i, col)) {
                    QColor bg = defaultBg;
                    if (col == 2 && mnBg.isValid()) bg = mnBg;
                    else if (col == 3 && operBg.isValid()) bg = operBg;
                    it->setBackground(bg);
                    switch (col) {
                    case 0: it->setForeground(addrColor); break;
                    case 1: it->setForeground(bytesColor); break;
                    case 2: it->setForeground(colorForMnemonic(line.mnemonic)); break;
                    case 3: it->setForeground(opColor); break;
                    case 4: it->setForeground(commentColor); break;
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

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    int viewHeight = viewport()->height();
    int hScroll = horizontalScrollBar()->value();

    // Draw vertical separator lines between columns (x64dbg style)
    QColor sepColor = ConfigColor("TableGridColor");
    painter.setPen(QPen(sepColor, 1));

    int xOffset = -hScroll;
    for (int col = 0; col < columnCount() - 1; col++) {
        xOffset += columnWidth(col);
        if (col == 2) continue;  // skip separator after Mnemonic
        painter.drawLine(xOffset - 1, 0, xOffset - 1, viewHeight);
    }

    // ── Inline jump graphics (x64dbg-style) ──
    if (m_lines.isEmpty()) return;

    QColor arrowColor = ConfigColor("DisassemblyJumpArrowColor");
    int rowHeight = verticalHeader()->defaultSectionSize();

    // X positions within the Bytes column
    int bytesColLeft = columnWidth(0) - hScroll;
    int smallArrowX = bytesColLeft + 4;   // small triangle indicators
    int lineX       = bytesColLeft + 12;  // full flow line for selected jump

    // ── 1. Small triangle indicators for ALL jump instructions ──
    for (int i = 0; i < m_lines.size(); i++) {
        const auto& line = m_lines[i];
        if (line.branchTarget == 0 || !line.mnemonic.startsWith('j'))
            continue;

        int rowTop = rowViewportPosition(i);
        if (rowTop < -rowHeight || rowTop > viewHeight)
            continue;

        int cy = rowTop + rowHeight / 2;
        int sz = 3;

        painter.setPen(Qt::NoPen);
        painter.setBrush(arrowColor);

        if (line.branchTarget < line.address) {
            QPoint tri[3] = {
                QPoint(smallArrowX, cy - sz),
                QPoint(smallArrowX - sz, cy + sz),
                QPoint(smallArrowX + sz, cy + sz),
            };
            painter.drawPolygon(tri, 3);
        } else {
            QPoint tri[3] = {
                QPoint(smallArrowX, cy + sz),
                QPoint(smallArrowX - sz, cy - sz),
                QPoint(smallArrowX + sz, cy - sz),
            };
            painter.drawPolygon(tri, 3);
        }
    }

    // ── 2. Full flow line for the SELECTED jump instruction ──
    int selRow = currentRow();
    if (selRow < 0 || selRow >= m_lines.size())
        return;

    const auto& selLine = m_lines[selRow];
    if (!selLine.mnemonic.startsWith('j') || selLine.branchTarget == 0)
        return;

    // Find destination row
    int destRow = -1;
    for (int j = 0; j < m_lines.size(); j++) {
        if (m_lines[j].address == selLine.branchTarget) {
            destRow = j;
            break;
        }
    }

    // Compute source and destination Y directly
    int srcY = rowViewportPosition(selRow) + rowHeight / 2;
    int destY;
    bool destOnScreen;

    if (destRow >= 0) {
        // Destination found within loaded lines
        destY = rowViewportPosition(destRow) + rowHeight / 2;
        destOnScreen = (destY >= 0 && destY <= viewHeight);
    } else if (selLine.branchTarget < selLine.address) {
        // Target is above — line goes to top of viewport
        destY = 0;
        destOnScreen = false;
    } else {
        // Target is below — line goes to bottom of viewport
        destY = viewHeight;
        destOnScreen = false;
    }

    // Red color for selected jump flow line
    QColor flowColor = ConfigColor("SideBarJumpSelectedColor");
    QPen flowPen(flowColor, 1.5);
    painter.setPen(flowPen);
    painter.setBrush(Qt::NoBrush);

    // Source: horizontal connector
    painter.drawLine(lineX, srcY, lineX + 5, srcY);

    // Vertical line from source to destination
    painter.drawLine(lineX, srcY, lineX, destY);

    // Destination: horizontal connector + arrowhead (only if on-screen)
    if (destOnScreen && destRow >= 0) {
        painter.drawLine(lineX, destY, lineX + 5, destY);

        // Arrowhead pointing right
        QPoint pts[3] = {
            QPoint(lineX + 3, destY - 2),
            QPoint(lineX + 5, destY),
            QPoint(lineX + 3, destY + 2),
        };
        painter.drawPolyline(pts, 3);
    }
}

void CPUDisassembly::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        int row = currentRow();
        if (row >= 0 && row < m_lines.size()) {
            const auto& line = m_lines[row];
            bool isBranch = line.mnemonic.startsWith('j') ||
                            line.mnemonic == "call" || line.mnemonic == "callq";
            if (isBranch) {
                // Parse target address from operands (e.g. "0x401000" or "401000")
                QString op = line.operands.trimmed();
                if (op.startsWith("0x", Qt::CaseInsensitive))
                    op = op.mid(2);
                bool ok;
                uint64_t target = op.toULongLong(&ok, 16);
                if (ok && target != 0)
                    goToAddress(target);
            }
        }
        return;
    }
    QTableWidget::keyPressEvent(event);
}

uint64_t CPUDisassembly::selectedAddress() const
{
    int row = currentRow();
    if (row >= 0 && row < m_lines.size())
        return m_lines[row].address;
    return 0;
}

void CPUDisassembly::wheelEvent(QWheelEvent* event)
{
    // Grab focus on wheel so scrolling works without clicking first
    if (!hasFocus())
        setFocus(Qt::MouseFocusReason);
    QTableWidget::wheelEvent(event);
}

// ── Column resize via separator dragging (x64dbg-style) ──

static constexpr int RESIZE_GRIP = 4;  // pixels from column edge to trigger resize

int CPUDisassembly::columnBoundaryAt(int x) const
{
    int hScroll = horizontalScrollBar()->value();
    int edge = -hScroll;
    // Check columns 0..3 (not the last stretch column)
    for (int col = 0; col < columnCount() - 1; col++) {
        edge += columnWidth(col);
        if (qAbs(x - edge) <= RESIZE_GRIP)
            return col;
    }
    return -1;
}

void CPUDisassembly::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int col = columnBoundaryAt(event->pos().x());
        if (col >= 0) {
            m_resizeCol = col;
            m_resizeDragStartX = event->pos().x();
            m_resizeOrigWidth = columnWidth(col);
            event->accept();
            return;
        }
    }
    QTableWidget::mousePressEvent(event);
}

void CPUDisassembly::mouseMoveEvent(QMouseEvent* event)
{
    if (m_resizeCol >= 0) {
        // Dragging a column boundary
        int delta = event->pos().x() - m_resizeDragStartX;
        int newWidth = qMax(horizontalHeader()->minimumSectionSize(),
                            m_resizeOrigWidth + delta);
        setColumnWidth(m_resizeCol, newWidth);
        viewport()->update();
        event->accept();
        return;
    }

    // Update cursor when hovering near a column boundary
    int col = columnBoundaryAt(event->pos().x());
    if (col >= 0)
        viewport()->setCursor(Qt::SplitHCursor);
    else
        viewport()->unsetCursor();

    QTableWidget::mouseMoveEvent(event);
}

void CPUDisassembly::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_resizeCol >= 0) {
        m_resizeCol = -1;
        viewport()->unsetCursor();
        event->accept();
        return;
    }
    QTableWidget::mouseReleaseEvent(event);
}

void CPUDisassembly::scrollContentsBy(int dx, int dy)
{
    QTableWidget::scrollContentsBy(dx, dy);

    // When scrolling down and near the bottom, load more instructions
    if (dy < 0 && !m_lines.isEmpty()) {  // dy < 0 means content moves up = scrolling down
        QScrollBar* vbar = verticalScrollBar();
        if (vbar && vbar->value() >= vbar->maximum() - 2)
            loadMoreBelow();
    }
}

void CPUDisassembly::loadMoreBelow()
{
    if (m_lines.isEmpty()) return;

    // Disassemble starting from the instruction after the last one we have
    const auto& lastLine = m_lines.last();
    uint64_t nextAddr = lastLine.address + lastLine.bytes.size();
    if (nextAddr == 0) return;

    auto more = m_debugCore->disassemble(nextAddr, 64);
    if (more.isEmpty()) return;

    // Append new lines and rebuild the table
    m_lines.append(more);
    rebuildTable();
    emit linesChanged();
}

void CPUDisassembly::promptSetLabel()
{
    uint64_t addr = selectedAddress();
    if (addr == 0) return;

    QString current = m_debugCore->getLabel(addr);
    bool ok;
    QString label = QInputDialog::getText(
        this, "Label",
        QString("Label for %1:").arg(addr, 16, 16, QChar('0')).toUpper(),
        QLineEdit::Normal, current, &ok);

    if (ok)
        m_debugCore->setLabel(addr, label.trimmed());
}
