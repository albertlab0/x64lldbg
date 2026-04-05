#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QTabBar>
#include "core/DebugCore.h"

class CPUDumpView : public QTableWidget
{
    Q_OBJECT

public:
    explicit CPUDumpView(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void refresh();
    void goToAddress(uint64_t address);

private:
    void setupColumns();
    void populate();
    void applyStyle();
    void paintEvent(QPaintEvent* event) override;

    DebugCore* m_debugCore;
    uint64_t m_baseAddress = 0x00400000;
};

// Tabbed container with Dump 1-5 tabs, like x64dbg
class CPUDump : public QWidget
{
    Q_OBJECT

public:
    explicit CPUDump(DebugCore* debugCore, QWidget* parent = nullptr);

    CPUDumpView* currentDumpView() const;

public slots:
    void refresh();
    void goToAddress(uint64_t address);

private:
    QTabBar* m_tabBar;
    CPUDumpView* m_dumpViews[5];
    int m_currentTab = 0;
    DebugCore* m_debugCore;
};
