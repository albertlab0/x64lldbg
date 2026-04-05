#include "CPUSideBar.h"
#include "common/Configuration.h"

#include <QPainter>
#include <QPaintEvent>
#include <QSet>
#include <QTableWidget>
#include <QHeaderView>
#include <QFontMetrics>

CPUSideBar::CPUSideBar(DebugCore* debugCore, QWidget* parent)
    : QWidget(parent)
    , m_debugCore(debugCore)
{
    setFixedWidth(55);
    setMinimumHeight(100);
}

void CPUSideBar::refresh()
{
    update();  // triggers repaint
}

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

    // Font for the RIP label
    QFont labelFont = ConfigFont("Disassembly");
    labelFont.setPointSize(labelFont.pointSize() - 1);
    labelFont.setBold(true);
    QFontMetrics fm(labelFont);

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
        int centerX = width() / 2;

        if (isBP) {
            // Breakpoint: red filled circle
            painter.setPen(Qt::NoPen);
            painter.setBrush(bpColor);
            painter.drawEllipse(QPoint(centerX, y), 6, 6);
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
            painter.drawEllipse(QPoint(centerX, y), 2, 2);
        }
    }
}
