#pragma once

#include <QWidget>
#include <QVector>
#include "core/DebugCore.h"

class QTableWidget;

class CPUSideBar : public QWidget
{
    Q_OBJECT

public:
    explicit CPUSideBar(DebugCore* debugCore, QWidget* parent = nullptr);

    void setRowHeight(int height) { m_rowHeight = height; update(); }
    void setLines(const QVector<DisassemblyLine>& lines) { m_lines = lines; update(); }
    void setTableWidget(QTableWidget* table) { m_table = table; }
    void setSelectedAddress(uint64_t addr) { m_selectedAddress = addr; update(); }

public slots:
    void refresh();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct JumpLine {
        int srcRow;
        int destRow;        // -1 = above viewport, m_lines.size() = below
        int lane = 0;
        bool isConditional;
        bool isSelected;
        bool isAtIP = false;   // this jump is at the current instruction pointer
        bool isTaken = false;  // conditional jump will be taken (based on RFLAGS)
    };

    void collectJumps(QVector<JumpLine>& jumps);
    void allocateLanes(QVector<JumpLine>& jumps);
    void drawJump(QPainter& painter, const JumpLine& jmp, int headerHeight, int arrowRightX);
    static bool evaluateJumpTaken(const QString& mnemonic, uint64_t rflags);

    DebugCore* m_debugCore;
    QTableWidget* m_table = nullptr;
    int m_rowHeight = 18;
    QVector<DisassemblyLine> m_lines;
    uint64_t m_selectedAddress = 0;
};
