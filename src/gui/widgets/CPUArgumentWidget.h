#pragma once

#include <QTableWidget>
#include "core/DebugCore.h"

class CPUArgumentWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit CPUArgumentWidget(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void setupColumns();
    void applyStyle();
    QString formatValue(uint64_t value);

    DebugCore* m_debugCore;
};
