#pragma once

#include <QTableWidget>
#include "core/DebugCore.h"

class CPURegistersView : public QTableWidget
{
    Q_OBJECT

public:
    explicit CPURegistersView(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void setupColumns();
    void applyStyle();

    DebugCore* m_debugCore;
};
