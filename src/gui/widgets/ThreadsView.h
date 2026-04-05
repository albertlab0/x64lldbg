#pragma once

#include <QTableWidget>
#include "core/DebugCore.h"

class ThreadsView : public QTableWidget
{
    Q_OBJECT

public:
    explicit ThreadsView(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void setupColumns();
    void applyStyle();

    DebugCore* m_debugCore;
};
