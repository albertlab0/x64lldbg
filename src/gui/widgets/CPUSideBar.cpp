#include "CPUSideBar.h"
#include "common/Configuration.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSet>
#include <QTableWidget>
#include <QHeaderView>
#include <QFontMetrics>
#include <algorithm>

static constexpr int LABEL_AREA_WIDTH = 40;  // left zone for bullets/BP/RIP
static constexpr int LANE_SPACING = 11;      // pixels between jump lanes

CPUSideBar::CPUSideBar(DebugCore* debugCore, QWidget* parent)
    : QWidget(parent)
    , m_debugCore(debugCore)
{
    setFixedWidth(100);
    setMinimumHeight(100);
}

void CPUSideBar::refresh()
{
    // Cache register state so paintEvent uses consistent, current data.
    // This is called via QueuedConnection after all register reads complete.
    m_cachedPC = m_debugCore->currentPC();
    m_cachedRFLAGS = m_debugCore->currentRFLAGS();
    qDebug("[SideBar::refresh] cachedPC=0x%llx cachedRFLAGS=0x%llx", m_cachedPC, m_cachedRFLAGS);
    update();
}

// ── Jump data collection ────────────────────────────────────────────

void CPUSideBar::collectJumps(QVector<JumpLine>& jumps)
{
    if (m_lines.isEmpty()) return;

    uint64_t firstAddr = m_lines.first().address;
    uint64_t lastAddr  = m_lines.last().address;
    uint64_t pc = m_cachedPC;
    uint64_t rflags = m_cachedRFLAGS;

    for (int i = 0; i < m_lines.size(); i++) {
        const auto& line = m_lines[i];

        // Only draw arrows for jump instructions (j*), not call
        if (!line.mnemonic.startsWith('j') || line.branchTarget == 0)
            continue;

        JumpLine jmp;
        jmp.srcRow = i;
        jmp.isConditional = (line.mnemonic != "jmp" && line.mnemonic != "jmpq");
        jmp.isSelected = (line.address == m_selectedAddress);
        jmp.isAtIP = (line.address == pc);

        // Evaluate taken/not-taken for jump at RIP or selected jump
        if ((jmp.isAtIP || jmp.isSelected) && jmp.isConditional) {
            jmp.isTaken = evaluateJumpTaken(line.mnemonic, rflags);
            emit m_debugCore->outputReceived(
                QString("[SideBar] Jump '%1' at 0x%2: isAtIP=%3 isSelected=%4 rflags=0x%5 isTaken=%6")
                .arg(line.mnemonic)
                .arg(line.address, 0, 16)
                .arg(jmp.isAtIP)
                .arg(jmp.isSelected)
                .arg(rflags, 0, 16)
                .arg(jmp.isTaken));
        } else if ((jmp.isAtIP || jmp.isSelected) && !jmp.isConditional)
            jmp.isTaken = true;  // unconditional jump is always taken

        uint64_t target = line.branchTarget;

        if (target < firstAddr) {
            jmp.destRow = -1;  // above viewport
        } else if (target > lastAddr) {
            jmp.destRow = m_lines.size();  // below viewport
        } else {
            // Find matching row
            jmp.destRow = -1;
            for (int j = 0; j < m_lines.size(); j++) {
                if (m_lines[j].address == target) {
                    jmp.destRow = j;
                    break;
                }
                if (m_lines[j].address > target) {
                    jmp.destRow = j;  // closest row after target
                    break;
                }
            }
        }

        jumps.append(jmp);
    }
}

bool CPUSideBar::evaluateJumpTaken(const QString& mnemonic, uint64_t rflags)
{
    // x86 RFLAGS bits — matches CPUInfoBox::isJumpTaken exactly
    bool CF = (rflags >> 0) & 1;
    bool PF = (rflags >> 2) & 1;
    bool ZF = (rflags >> 6) & 1;
    bool SF = (rflags >> 7) & 1;
    bool OF = (rflags >> 11) & 1;

    // Use raw mnemonic (no stripping) — LLDB uses "jge" not "jgeq"
    if (mnemonic == "jo")                                       return OF;
    if (mnemonic == "jno")                                      return !OF;
    if (mnemonic == "jb" || mnemonic == "jnae" || mnemonic == "jc")  return CF;
    if (mnemonic == "jnb" || mnemonic == "jae" || mnemonic == "jnc") return !CF;
    if (mnemonic == "jz" || mnemonic == "je")                   return ZF;
    if (mnemonic == "jnz" || mnemonic == "jne")                 return !ZF;
    if (mnemonic == "jbe" || mnemonic == "jna")                 return CF || ZF;
    if (mnemonic == "jnbe" || mnemonic == "ja")                 return !CF && !ZF;
    if (mnemonic == "js")                                       return SF;
    if (mnemonic == "jns")                                      return !SF;
    if (mnemonic == "jp" || mnemonic == "jpe")                  return PF;
    if (mnemonic == "jnp" || mnemonic == "jpo")                 return !PF;
    if (mnemonic == "jl" || mnemonic == "jnge")                 return SF != OF;
    if (mnemonic == "jnl" || mnemonic == "jge")                 return SF == OF;
    if (mnemonic == "jle" || mnemonic == "jng")                 return ZF || (SF != OF);
    if (mnemonic == "jnle" || mnemonic == "jg")                 return !ZF && (SF == OF);

    return false;
}

// ── Lane allocation (prevents overlapping arrows) ───────────────────

void CPUSideBar::allocateLanes(QVector<JumpLine>& jumps)
{
    if (jumps.isEmpty()) return;

    int rowCount = m_lines.size();

    // Sort by span length — longer jumps get outer lanes
    std::sort(jumps.begin(), jumps.end(), [](const JumpLine& a, const JumpLine& b) {
        return qAbs(a.destRow - a.srcRow) > qAbs(b.destRow - b.srcRow);
    });

    // Track maximum lane used at each row
    QVector<int> maxLane(rowCount, 0);

    for (auto& jmp : jumps) {
        int minRow = qBound(0, qMin(jmp.srcRow, jmp.destRow), rowCount - 1);
        int maxRow = qBound(0, qMax(jmp.srcRow, jmp.destRow), rowCount - 1);

        int maxUsed = 0;
        for (int r = minRow; r <= maxRow; r++)
            maxUsed = qMax(maxUsed, maxLane[r]);

        jmp.lane = maxUsed + 1;

        for (int r = minRow; r <= maxRow; r++)
            maxLane[r] = jmp.lane;
    }
}

// ── Draw a single jump arrow ────────────────────────────────────────

void CPUSideBar::drawJump(QPainter& painter, const JumpLine& jmp,
                          int headerHeight, int arrowRightX)
{
    if (!m_table) return;

    // Lane X position: lane 1 closest to right edge, deeper lanes go left
    int laneX = arrowRightX - jmp.lane * LANE_SPACING;

    // Y positions
    int srcY = headerHeight + m_table->rowViewportPosition(jmp.srcRow) + m_rowHeight / 2;

    int destY;
    bool destOffTop = (jmp.destRow < 0);
    bool destOffBot = (jmp.destRow >= m_lines.size());

    if (destOffTop) {
        destY = headerHeight;
    } else if (destOffBot) {
        destY = height();
    } else {
        destY = headerHeight + m_table->rowViewportPosition(jmp.destRow) + m_rowHeight / 2;
    }

    // Self-loop: expand Y range slightly
    if (jmp.srcRow == jmp.destRow) {
        srcY -= m_rowHeight / 4;
        destY += m_rowHeight / 4;
    }

    // Set pen: at-IP-taken = bold red, at-IP-not-taken = gray,
    // selected = bold red, unselected = thin muted
    QColor selectedColor = ConfigColor("SideBarJumpSelectedColor");
    QColor unselectedColor = ConfigColor("SideBarJumpLineColor");
    QColor notTakenColor = ConfigColor("SideBarJumpNotTakenColor");

    QPen pen;
    const char* colorName = "default";
    if ((jmp.isAtIP || jmp.isSelected) && !jmp.isTaken) {
        // Jump at RIP or selected, NOT taken: gray
        pen = QPen(notTakenColor, 2.0);
        colorName = "GRAY(not-taken)";
    } else if (jmp.isAtIP || jmp.isSelected) {
        // Taken: red
        pen = QPen(selectedColor, 2.0);
        colorName = "RED(taken)";
    } else {
        pen = QPen(unselectedColor, 1.0);
        colorName = "default(unselected)";
    }
    if (jmp.isAtIP || jmp.isSelected)
        qDebug("[SideBar::drawJump] row=%d %s isAtIP=%d isSel=%d isTaken=%d color=%s",
               jmp.srcRow, colorName, jmp.isAtIP, jmp.isSelected, jmp.isTaken,
               qPrintable(pen.color().name()));
    if (jmp.isConditional)
        pen.setStyle(Qt::DashLine);
    else
        pen.setStyle(Qt::SolidLine);

    painter.setPen(pen);

    // Draw the arrow body as a single path so the dash pattern is continuous
    QPainterPath path;
    path.moveTo(arrowRightX, srcY);
    path.lineTo(laneX, srcY);
    path.lineTo(laneX, destY);
    if (!destOffTop && !destOffBot)
        path.lineTo(arrowRightX, destY);
    painter.drawPath(path);

    // Arrowhead (always solid)
    QPen arrowPen = pen;
    arrowPen.setStyle(Qt::SolidLine);
    painter.setPen(arrowPen);

    if (!destOffTop && !destOffBot) {
        // Arrowhead pointing right (toward disassembly)
        int sz = 4;
        painter.drawLine(arrowRightX, destY, arrowRightX - sz, destY - sz);
        painter.drawLine(arrowRightX, destY, arrowRightX - sz, destY + sz);
    } else if (destOffTop) {
        // Arrow pointing up at top edge
        int sz = 3;
        painter.drawLine(laneX, destY, laneX - sz, destY + sz + 1);
        painter.drawLine(laneX, destY, laneX + sz, destY + sz + 1);
    } else {
        // Arrow pointing down at bottom edge
        int sz = 3;
        painter.drawLine(laneX, destY, laneX - sz, destY - sz - 1);
        painter.drawLine(laneX, destY, laneX + sz, destY - sz - 1);
    }
}

// ── Main paint ──────────────────────────────────────────────────────

static int s_paintSeq = 0;

void CPUSideBar::paintEvent(QPaintEvent* event)
{
    int seq = ++s_paintSeq;
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    qDebug("[SideBar::paint#%d] cachedPC=0x%llx cachedRFLAGS=0x%llx selectedAddr=0x%llx",
           seq, m_cachedPC, m_cachedRFLAGS, m_selectedAddress);

    QColor bg = ConfigColor("SideBarBackgroundColor");
    QColor bulletColor = ConfigColor("SideBarBulletColor");
    QColor bpColor = ConfigColor("BreakpointColor");

    // x64dbg CIP label colors: blue background (#4040FF), white text
    QColor cipLabelBg = QColor("#4040FF");
    QColor cipLabelFg = QColor("#FFFFFF");

    painter.fillRect(rect(), bg);

    if (m_rowHeight <= 0 || m_lines.isEmpty() || !m_table)
        return;

    uint64_t pc = m_cachedPC;

    // Get breakpoint addresses
    QSet<uint64_t> bpAddresses;
    auto bps = m_debugCore->getBreakpoints();
    for (const auto& bp : bps) {
        if (bp.enabled)
            bpAddresses.insert(bp.address);
    }

    // Get the header height to align with table rows
    int headerHeight = m_table->horizontalHeader()->height();

    // ── Draw jump arrows (background layer) ──
    int arrowRightX = width() - 8;  // keep arrows clear of opcode area

    QVector<JumpLine> jumps;
    collectJumps(jumps);
    allocateLanes(jumps);

    // Draw in 3 passes: normal → selected → isAtIP (highest priority on top)
    for (const auto& jmp : jumps) {
        if (!jmp.isSelected && !jmp.isAtIP)
            drawJump(painter, jmp, headerHeight, arrowRightX);
    }
    for (const auto& jmp : jumps) {
        if (jmp.isSelected && !jmp.isAtIP)
            drawJump(painter, jmp, headerHeight, arrowRightX);
    }
    for (const auto& jmp : jumps) {
        if (jmp.isAtIP)
            drawJump(painter, jmp, headerHeight, arrowRightX);
    }

    // ── Draw bullets, breakpoints, and RIP label (foreground) ──

    // Font for the RIP label
    QFont labelFont = ConfigFont("Disassembly");
    labelFont.setPointSize(labelFont.pointSize() - 1);
    labelFont.setBold(true);
    QFontMetrics fm(labelFont);

    int bulletCenterX = LABEL_AREA_WIDTH / 2;

    for (int i = 0; i < m_lines.size(); i++) {
        // Use the table's actual row position for pixel-perfect alignment
        int rowTop = m_table->rowViewportPosition(i);
        if (rowTop < 0 && i > 0) continue;  // row scrolled out
        int y = headerHeight + rowTop + m_rowHeight / 2;
        if (y < 0 || y > height())
            continue;

        uint64_t addr = m_lines[i].address;
        bool isIP = (addr == pc);
        bool isBP = bpAddresses.contains(addr);

        if (isBP) {
            // Breakpoint: red filled circle
            painter.setPen(Qt::NoPen);
            painter.setBrush(bpColor);
            painter.drawEllipse(QPoint(bulletCenterX, y), 6, 6);
        }

        if (isIP) {
            // x64dbg-style CIP: blue "RIP" label box + long horizontal arrow
            painter.save();

            QString label = "RIP";
            int textWidth = fm.horizontalAdvance(label) + 6; // padding
            int boxHeight = m_rowHeight - 2;
            int boxX = 1;
            int boxY = y - boxHeight / 2;

            // Draw blue rectangle with "RIP" text
            QRect labelRect(boxX, boxY, textWidth, boxHeight);
            painter.setPen(Qt::NoPen);
            painter.setBrush(cipLabelBg);
            painter.drawRect(labelRect);

            painter.setFont(labelFont);
            painter.setPen(cipLabelFg);
            painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignVCenter, label);

            // Draw horizontal arrow line from label box to the right edge
            int arrowStartX = boxX + textWidth;
            int arrowEndX = width() - 8;
            int arrowY = y;

            QPen arrowPen(cipLabelBg, 2.0);
            painter.setPen(arrowPen);
            painter.drawLine(arrowStartX, arrowY, arrowEndX, arrowY);

            // Draw arrowhead (V-shape)
            int arrowSize = 4;
            painter.drawLine(arrowEndX - 1, arrowY - 1, arrowEndX - arrowSize, arrowY - arrowSize);
            painter.drawLine(arrowEndX - 1, arrowY,     arrowEndX - arrowSize, arrowY + arrowSize - 1);

            painter.restore();
        } else if (!isBP) {
            // Regular bullet (only if not BP and not IP)
            painter.setPen(Qt::NoPen);
            painter.setBrush(bulletColor);
            painter.drawEllipse(QPoint(bulletCenterX, y), 2, 2);
        }
    }
}
