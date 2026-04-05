#pragma once

#include <QTableWidget>
#include "core/DebugCore.h"

class MemoryMapView : public QTableWidget
{
    Q_OBJECT

public:
    explicit MemoryMapView(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void setupColumns();
    void applyStyle();

    DebugCore* m_debugCore;
};
