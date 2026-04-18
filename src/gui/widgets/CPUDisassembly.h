#pragma once

#include <QTableWidget>
#include "core/DebugCore.h"

class CPUDisassembly : public QTableWidget
{
    Q_OBJECT

public:
    explicit CPUDisassembly(DebugCore* debugCore, QWidget* parent = nullptr);

    uint64_t selectedAddress() const;
    const QVector<DisassemblyLine>& getLines() const { return m_lines; }

public slots:
    void refresh();
    void goToAddress(uint64_t address);

signals:
    void addressSelected(uint64_t address);
    void linesChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void setupColumns();
    void applyStyle();
    void setupContextMenu();
    void populateFromAddress(uint64_t address);
    void rebuildTable();
    void loadMoreAbove();
    void loadMoreBelow();
    void updateHighlights(uint64_t pc);
    void promptSetLabel();
    void promptEditBreakpoint();
    QColor colorForMnemonic(const QString& mnemonic) const;
    QColor bgColorForMnemonic(const QString& mnemonic) const;
    int columnBoundaryAt(int x) const;  // returns col index if x is near a boundary, else -1

    DebugCore* m_debugCore;
    uint64_t m_baseAddress = 0;
    uint64_t m_gotoAddress = 0;  // user navigation highlight (Ctrl+G)
    bool m_flowRepaintPending = false;  // guards against repaint loops
    bool m_hadFlowLine = false;         // flow line was drawn in last full paint
    QVector<DisassemblyLine> m_lines;

    // Column resize dragging (x64dbg-style, no visible header)
    int m_resizeCol = -1;       // column being resized (-1 = none)
    int m_resizeDragStartX = 0; // mouse X at drag start
    int m_resizeOrigWidth = 0;  // column width at drag start
    bool m_suppressAutoScroll = false; // guard against cascading scroll during goto
};
