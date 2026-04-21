#include "CPUStack.h"
#include "common/Configuration.h"

#include <QHeaderView>
#include <QPainter>
#include <algorithm>

// ── Bracket delegate ─────────────────────────────────────────────────

void StackFrameBracketDelegate::paint(QPainter* painter,
                                      const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    // Paint the underlying cell first (text, background, selection).
    QStyledItemDelegate::paint(painter, option, index);

    int row = index.row();
    if (row < 0 || row >= m_bits.size()) return;
    int bits = m_bits[row];
    if (bits == 0) return;

    QColor color = ConfigColor("StackFrameColor");
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen pen(color, 2);
    painter->setPen(pen);

    // Bracket geometry: 5px-wide hook drawn at x = cell_left + offset.
    const int offset = 4;
    const int width  = 5;
    const QRect r = option.rect;
    int x     = r.left() + offset;
    int xEnd  = x + width;
    int top   = r.top();
    int bot   = r.bottom();
    int mid   = r.top() + r.height() / 2;

    // Top of a frame: `┌` — half vertical from mid→bottom, horizontal
    // connector at mid.
    if (bits & 1) {
        painter->drawLine(x, mid, x, bot);
        painter->drawLine(x, mid, xEnd, mid);
    }
    // Bottom of a frame: `└` — half vertical from top→mid, horizontal
    // connector at mid.
    if (bits & 2) {
        painter->drawLine(x, top, x, mid);
        painter->drawLine(x, mid, xEnd, mid);
    }
    // Middle: full vertical spine.
    if (bits & 4) {
        painter->drawLine(x, top, x, bot);
    }

    painter->restore();
}

// ── CPUStack ─────────────────────────────────────────────────────────

CPUStack::CPUStack(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
    , m_bracketDelegate(new StackFrameBracketDelegate(this))
{
    setupColumns();
    applyStyle();
    setItemDelegateForColumn(1, m_bracketDelegate);
    refresh();
}

void CPUStack::setupColumns()
{
    setColumnCount(3);
    horizontalHeader()->setVisible(false);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(18);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setShowGrid(false);
    setAlternatingRowColors(false);
}

void CPUStack::applyStyle()
{
    QFont font = ConfigFont("Stack");
    setFont(font);

    QColor bg = ConfigColor("StackBackgroundColor");
    QColor fg = ConfigColor("StackTextColor");
    QColor sel = ConfigColor("DisassemblySelectionColor");
    QColor alt = ConfigColor("AlternateRowColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: none; outline: none; }"
        "QTableWidget::item { padding: 0 4px; }"
        "QTableWidget::item:selected { background-color: %3; }"
        "QTableWidget::item:alternate { background-color: %4; }"
    ).arg(bg.name(), fg.name(), sel.name(), alt.name()));
}

void CPUStack::refresh()
{
    auto entries = m_debugCore->getStackEntries(24);
    setRowCount(entries.size());

    // Pull call stack so we can bracket each frame. Frames are returned
    // innermost-first; sort by CFA ascending — matches x64dbg's approach.
    QVector<uint64_t> cfas;
    auto frames = m_debugCore->getCallStack();
    for (const auto& f : frames)
        if (f.cfa != 0)
            cfas.push_back(f.cfa);
    std::sort(cfas.begin(), cfas.end());

    // Compute per-row frame bitfield. Each frame f owns stack addresses
    // [cfas[f-1] + ptrSize, cfas[f]] (or [sp, cfas[0]] for the innermost
    // frame). The CFA itself is the bottom of its frame; one slot above
    // the previous CFA is the top of the current frame.
    QVector<int> bits(entries.size(), 0);
    const uint64_t ptrSize = 8;
    uint64_t sp = entries.isEmpty() ? 0 : entries[0].address;
    for (int i = 0; i < entries.size(); ++i) {
        uint64_t va = entries[i].address;
        for (int f = 0; f < cfas.size(); ++f) {
            uint64_t frameBot   = cfas[f];
            uint64_t frameTop   = (f == 0) ? sp : cfas[f - 1] + ptrSize;
            if (va < frameTop || va > frameBot) continue;
            if (va == frameBot)      bits[i] |= 2;   // bottom (CFA row)
            else if (va == frameTop) bits[i] |= 1;   // top
            else                     bits[i] |= 4;   // middle spine
            break;
        }
    }
    m_bracketDelegate->setFrameBitfield(bits);

    QColor addrColor = ConfigColor("StackAddressColor");
    QColor valueColor = ConfigColor("StackTextColor");
    QColor spColor = ConfigColor("StackCurrentSPColor");
    QColor commentColor = ConfigColor("DisassemblyCommentColor");

    for (int i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];

        // x64dbg marks the SP row with a coloured address only — no
        // full-row background highlight.
        auto* addrItem = new QTableWidgetItem(
            QString("0x%1").arg(entry.address, 16, 16, QChar('0'))
        );
        addrItem->setForeground(i == 0 ? spColor : addrColor);
        setItem(i, 0, addrItem);

        auto* valueItem = new QTableWidgetItem(
            QString("0x%1").arg(entry.value, 16, 16, QChar('0'))
        );
        valueItem->setForeground(valueColor);
        setItem(i, 1, valueItem);

        auto* commentItem = new QTableWidgetItem(entry.comment);
        commentItem->setForeground(commentColor);
        setItem(i, 2, commentItem);
    }
}
