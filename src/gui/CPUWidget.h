#pragma once

#include <QWidget>

class QSplitter;
class DebugCore;
class CPUDisassembly;
class CPUInfoBox;
class CPUSideBar;
class CPURegistersView;
class CPUStack;
class CPUDump;
class CPUArgumentWidget;

class CPUWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CPUWidget(DebugCore* debugCore, QWidget* parent = nullptr);

    CPUDisassembly* getDisassembly() const { return m_disassembly; }
    CPURegistersView* getRegisters() const { return m_registers; }
    CPUStack* getStack() const { return m_stack; }
    CPUDump* getDump() const { return m_dump; }
    CPUInfoBox* getInfoBox() const { return m_infoBox; }
    CPUSideBar* getSideBar() const { return m_sideBar; }
    CPUArgumentWidget* getArguments() const { return m_arguments; }

private:
    void setupLayout();

    DebugCore* m_debugCore;

    // Splitters (matching x64dbg naming)
    QSplitter* m_vSplitter;
    QSplitter* m_topHSplitter;
    QSplitter* m_topLeftVSplitter;
    QSplitter* m_topLeftUpperHSplitter;
    QSplitter* m_topRightVSplitter;
    QSplitter* m_botHSplitter;

    // Widgets
    CPUSideBar*         m_sideBar;
    CPUDisassembly*     m_disassembly;
    CPUInfoBox*         m_infoBox;
    CPURegistersView*   m_registers;
    CPUArgumentWidget*  m_arguments;
    CPUDump*            m_dump;
    CPUStack*           m_stack;
};
