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

public slots:
    void refresh();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    DebugCore* m_debugCore;
    QTableWidget* m_table = nullptr;
    int m_rowHeight = 18;
    QVector<DisassemblyLine> m_lines;
};
