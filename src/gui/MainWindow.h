#pragma once

#include <QMainWindow>

class QTabWidget;
class DebugCore;
class CPUWidget;
class BreakpointsView;
class MemoryMapView;
class CallStackView;
class ThreadsView;
class LogView;
class QLabel;
class QToolBar;
class CommandLineEdit;
class ScriptView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DebugCore* debugCore, QWidget* parent = nullptr);

private:
    void createMenuBar();
    void createToolBar();
    void createCentralTabs();
    void createCommandBar();
    void createStatusBar();
    void connectSignals();
    void applyTheme();
    void updateStatusLabel(int state);

    DebugCore* m_debugCore;

    // Central tab widget (like x64dbg)
    QTabWidget* m_tabWidget;
    CPUWidget* m_cpuWidget;

    // Tab views
    BreakpointsView* m_breakpointsView;
    MemoryMapView*   m_memoryMapView;
    CallStackView*   m_callStackView;
    ThreadsView*     m_threadsView;
    LogView*         m_logView;
    ScriptView*      m_scriptView;

    // Toolbar
    QToolBar* m_toolBar;

    // Command bar
    CommandLineEdit* m_commandLine;

    // Status bar
    QLabel* m_statusLabel;

    // Actions
    QAction* m_actionRun;
    QAction* m_actionStepInto;
    QAction* m_actionStepOver;
    QAction* m_actionStepOut;
    QAction* m_actionRunToCursor;
    QAction* m_actionPause;
    QAction* m_actionRestart;
    QAction* m_actionStop;
    QAction* m_actionToggleBP;
};
