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

signals:
    void registerChanged(const QString& name, uint64_t newValue);

private:
    void setupColumns();
    void applyStyle();
    void editRegisterAt(int row);

    DebugCore* m_debugCore;
};
