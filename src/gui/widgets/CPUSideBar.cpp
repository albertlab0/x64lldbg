#include "CPUSideBar.h"
#include "common/Configuration.h"

#include <QPainter>
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
    update();  // triggers repaint
}

// ── Jump data collection ────────────────────────────────────────────

void CPUSideBar::collectJumps(QVector<JumpLine>& jumps)
{
    if (m_lines.isEmpty()) return;

    uint64_t firstAddr = m_lines.first().address;
    uint64_t lastAddr  = m_lines.last().address;

    for (int i = 0; i < m_lines.size(); i++) {
        const auto& line = m_lines[i];

        // Only draw arrows for jump instructions (j*), not call
        if (!line.mnemonic.startsWith('j') || line.branchTarget == 0)
            continue;

        JumpLine jmp;
        jmp.srcRow = i;
        jmp.isConditional = (line.mnemonic != "jmp" && line.mnemonic != "jmpq");
        jmp.isSelected = (line.address == m_selectedAddress);

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

    // Set pen: selected = bold red, unselected = thin muted
    QColor selectedColor = ConfigColor("SideBarJumpSelectedColor");
    QColor unselectedColor = ConfigColor("SideBarJumpLineColor");

    QPen pen;
    if (jmp.isSelected) {
        pen = QPen(selectedColor, 2.0);
    } else {
        pen = QPen(unselectedColor, 1.0);
    }
    if (jmp.isConditional)
        pen.setStyle(Qt::DashLine);
    else
        pen.setStyle(Qt::SolidLine);

    painter.setPen(pen);

    // Draw: source horizontal → vertical → dest horizontal + arrowhead
    // Source row: horizontal from right edge to lane
    painter.drawLine(arrowRightX, srcY, laneX, srcY);

    // Vertical connecting line
    painter.drawLine(laneX, srcY, laneX, destY);

    if (!destOffTop && !destOffBot) {
        // Destination row: horizontal from lane to right edge
        painter.drawLine(laneX, destY, arrowRightX, destY);

        // Arrowhead pointing right (toward disassembly)
        int sz = 4;
        // Use solid pen for arrowhead even on dashed lines
        QPen arrowPen = pen;
        arrowPen.setStyle(Qt::SolidLine);
        painter.setPen(arrowPen);
        painter.drawLine(arrowRightX, destY, arrowRightX - sz, destY - sz);
        painter.drawLine(arrowRightX, destY, arrowRightX - sz, destY + sz);
    } else if (destOffTop) {
        // Arrow pointing up at top edge
        int sz = 3;
        QPen arrowPen = pen;
        arrowPen.setStyle(Qt::SolidLine);
        painter.setPen(arrowPen);
        painter.drawLine(laneX, destY, laneX - sz, destY + sz + 1);
        painter.drawLine(laneX, destY, laneX + sz, destY + sz + 1);
    } else {
        // Arrow pointing down at bottom edge
        int sz = 3;
        QPen arrowPen = pen;
        arrowPen.setStyle(Qt::SolidLine);
        painter.setPen(arrowPen);
        painter.drawLine(laneX, destY, laneX - sz, destY - sz - 1);
        painter.drawLine(laneX, destY, laneX + sz, destY - sz - 1);
    }
}

// ── Main paint ──────────────────────────────────────────────────────

void CPUSideBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor bg = ConfigColor("SideBarBackgroundColor");
    QColor bulletColor = ConfigColor("SideBarBulletColor");
    QColor bpColor = ConfigColor("BreakpointColor");

    // x64dbg CIP label colors: blue background (#4040FF), white text
    QColor cipLabelBg = QColor("#4040FF");
    QColor cipLabelFg = QColor("#FFFFFF");

    painter.fillRect(rect(), bg);

    if (m_rowHeight <= 0 || m_lines.isEmpty() || !m_table)
        return;

    uint64_t pc = m_debugCore->currentPC();

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
    int arrowRightX = width() - 3;

    QVector<JumpLine> jumps;
    collectJumps(jumps);
    allocateLanes(jumps);

    // Draw non-selected jumps first, then selected on top
    for (const auto& jmp : jumps) {
        if (!jmp.isSelected)
            drawJump(painter, jmp, headerHeight, arrowRightX);
    }
    for (const auto& jmp : jumps) {
        if (jmp.isSelected)
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
            int arrowEndX = width() - 2;
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
