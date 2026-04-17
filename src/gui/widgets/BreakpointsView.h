#pragma once

#include <QTableWidget>
#include "core/DebugCore.h"

class BreakpointsView : public QTableWidget
{
    Q_OBJECT

public:
    explicit BreakpointsView(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void refresh();

signals:
    void breakpointDoubleClicked(uint64_t address);

private:
    void setupColumns();
    void applyStyle();
    void setupContextMenu();
    void editBreakpointAt(int row);

    DebugCore* m_debugCore;
};
