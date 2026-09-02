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

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupColumns();
    void applyStyle();
    void setupContextMenu();
    void editBreakpointAt(int row);
    void deleteSelectedBreakpoints();

    DebugCore* m_debugCore;
};
