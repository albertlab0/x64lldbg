#include "MainWindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QTabBar>
#include <QTabWidget>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QIcon>
#include <QStyle>

#include "core/DebugCore.h"
#include "common/Configuration.h"
#include "gui/CPUWidget.h"
#include "gui/widgets/CPUDisassembly.h"
#include "gui/widgets/CPUInfoBox.h"
#include "gui/widgets/CPURegistersView.h"
#include "gui/widgets/CPUStack.h"
#include "gui/widgets/CPUDump.h"
#include "gui/widgets/BreakpointsView.h"
#include "gui/widgets/MemoryMapView.h"
#include "gui/widgets/CallStackView.h"
#include "gui/widgets/ThreadsView.h"
#include "gui/dialogs/GotoDialog.h"
#include "gui/widgets/CPUSideBar.h"
#include "gui/widgets/LogView.h"
#include "gui/widgets/CommandLineEdit.h"
#include "gui/widgets/CPUArgumentWidget.h"

MainWindow::MainWindow(DebugCore* debugCore, QWidget* parent)
    : QMainWindow(parent)
    , m_debugCore(debugCore)
{
    setWindowTitle("x64lldbg");
    resize(1400, 900);

    createMenuBar();
    createToolBar();
    createCentralTabs();
    createCommandBar();
    createStatusBar();
    connectSignals();

    applyTheme();

    // Re-apply theme when it changes
    connect(Config(), &Configuration::themeChanged, this, &MainWindow::applyTheme);
}

void MainWindow::createMenuBar()
{
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Open...", this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Open Executable");
        if (path.isEmpty()) return;

        QString arch;
        QStringList archs = DebugCore::detectArchitectures(path);
        if (archs.size() > 1) {
            bool ok = false;
            arch = QInputDialog::getItem(this, "Select Architecture",
                "This is a universal binary.\nChoose architecture to debug:",
                archs, 0, false, &ok);
            if (!ok) return;
        }
        m_debugCore->startDebug(path, {}, arch);
    });
    fileMenu->addAction("&Attach to Process...", this, [this]() {
        // TODO: attach dialog
        Q_UNUSED(this)
    });
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);

    auto* viewMenu = menuBar()->addMenu("&View");
    // Dock widget toggles added after dock creation

    auto* debugMenu = menuBar()->addMenu("&Debug");

    m_actionRun = debugMenu->addAction("&Run",
        Config()->getShortcut("DebugRun").hotkey,
        m_debugCore, &DebugCore::continueExec);

    m_actionStepInto = debugMenu->addAction("Step &Into",
        Config()->getShortcut("DebugStepInto").hotkey,
        m_debugCore, &DebugCore::stepInto);

    m_actionStepOver = debugMenu->addAction("Step &Over",
        Config()->getShortcut("DebugStepOver").hotkey,
        m_debugCore, &DebugCore::stepOver);

    m_actionStepOut = debugMenu->addAction("Step O&ut",
        Config()->getShortcut("DebugStepOut").hotkey,
        m_debugCore, &DebugCore::stepOut);

    m_actionRunToCursor = debugMenu->addAction("Run to &Cursor",
        this, [this]() {
            uint64_t addr = m_cpuWidget->getDisassembly()->selectedAddress();
            if (addr != 0)
                m_debugCore->runToCursor(addr);
        });
    m_actionRunToCursor->setText("Run to &Cursor\t" +
        Config()->getShortcut("DebugRunToCursor").hotkey.toString(QKeySequence::NativeText));

    debugMenu->addSeparator();

    m_actionPause = debugMenu->addAction("&Pause",
        Config()->getShortcut("DebugPause").hotkey,
        m_debugCore, &DebugCore::pause);

    m_actionRestart = debugMenu->addAction("R&estart",
        Config()->getShortcut("DebugRestart").hotkey,
        m_debugCore, &DebugCore::restart);

    m_actionStop = debugMenu->addAction("&Stop",
        Config()->getShortcut("DebugClose").hotkey,
        m_debugCore, &DebugCore::stop);

    debugMenu->addSeparator();

    m_actionToggleBP = debugMenu->addAction("Toggle &Breakpoint",
        this, [this]() {
            uint64_t addr = m_cpuWidget->getDisassembly()->selectedAddress();
            if (addr != 0)
                m_debugCore->toggleBreakpoint(addr);
        });
    m_actionToggleBP->setText("Toggle &Breakpoint\t" +
        Config()->getShortcut("ToggleBreakpoint").hotkey.toString(QKeySequence::NativeText));

    auto* optionsMenu = menuBar()->addMenu("&Options");

    auto* themeMenu = optionsMenu->addMenu("&Theme");
    auto* x64dbgAction = themeMenu->addAction("x64dbg Default", this, [this]() {
        Config()->setTheme(Configuration::X64dbgDefault);
    });
    auto* modernAction = themeMenu->addAction("Modern Light", this, [this]() {
        Config()->setTheme(Configuration::ModernLight);
    });
    auto* cutterAction = themeMenu->addAction("Cutter Dark", this, [this]() {
        Config()->setTheme(Configuration::CutterDark);
    });
    x64dbgAction->setCheckable(true);
    modernAction->setCheckable(true);
    cutterAction->setCheckable(true);
    x64dbgAction->setChecked(Config()->currentTheme() == Configuration::X64dbgDefault);
    modernAction->setChecked(Config()->currentTheme() == Configuration::ModernLight);
    cutterAction->setChecked(Config()->currentTheme() == Configuration::CutterDark);

    // Update check marks when theme changes
    connect(Config(), &Configuration::themeChanged, this, [x64dbgAction, modernAction, cutterAction]() {
        x64dbgAction->setChecked(Config()->currentTheme() == Configuration::X64dbgDefault);
        modernAction->setChecked(Config()->currentTheme() == Configuration::ModernLight);
        cutterAction->setChecked(Config()->currentTheme() == Configuration::CutterDark);
    });

    optionsMenu->addSeparator();

    auto* syntaxMenu = optionsMenu->addMenu("&Assembly Syntax");
    auto* intelAction = syntaxMenu->addAction("Intel", this, [this]() {
        m_debugCore->setAsmFlavor(DebugCore::Intel);
    });
    auto* attAction = syntaxMenu->addAction("AT&&T", this, [this]() {
        m_debugCore->setAsmFlavor(DebugCore::ATT);
    });
    intelAction->setCheckable(true);
    attAction->setCheckable(true);
    intelAction->setChecked(m_debugCore->asmFlavor() == DebugCore::Intel);
    attAction->setChecked(m_debugCore->asmFlavor() == DebugCore::ATT);

    connect(m_debugCore, &DebugCore::asmFlavorChanged, this, [this, intelAction, attAction]() {
        intelAction->setChecked(m_debugCore->asmFlavor() == DebugCore::Intel);
        attAction->setChecked(m_debugCore->asmFlavor() == DebugCore::ATT);
        // Refresh disassembly with new syntax
        m_cpuWidget->getDisassembly()->refresh();
    });

    optionsMenu->addSeparator();
    optionsMenu->addAction("&Settings...", this, [](){
        // TODO: settings dialog
    });

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About x64lldbg", this, [this]() {
        QMessageBox::about(this, "About x64lldbg",
            "x64lldbg v0.1.0\n\n"
            "A cross-platform GUI debugger frontend for LLDB\n"
            "Inspired by x64dbg");
    });

    // Populate View menu with dock toggles (done in createDockWidgets)
    Q_UNUSED(viewMenu)
}

void MainWindow::createToolBar()
{
    m_toolBar = addToolBar("Debug");
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(20, 20));

    // Toolbar icons (48x48 PNGs, scaled down by Qt)
    m_actionRun->setIcon(QIcon(":/icons/run.png"));
    m_actionPause->setIcon(QIcon(":/icons/pause.png"));
    m_actionStepInto->setIcon(QIcon(":/icons/step-into.png"));
    m_actionStepOver->setIcon(QIcon(":/icons/step-over.png"));
    m_actionStepOut->setIcon(QIcon(":/icons/step-out.png"));
    m_actionRunToCursor->setIcon(QIcon(":/icons/run-to-cursor.png"));
    m_actionRestart->setIcon(QIcon(":/icons/restart.png"));
    m_actionStop->setIcon(QIcon(":/icons/stop.png"));
    m_actionToggleBP->setIcon(QIcon(":/icons/breakpoint.png"));

    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_toolBar->addAction(m_actionRun);
    m_toolBar->addAction(m_actionPause);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_actionStepInto);
    m_toolBar->addAction(m_actionStepOver);
    m_toolBar->addAction(m_actionStepOut);
    m_toolBar->addAction(m_actionRunToCursor);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_actionRestart);
    m_toolBar->addAction(m_actionStop);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_actionToggleBP);
}

void MainWindow::createCentralTabs()
{
    // x64dbg-style: top-level tabs with CPU as first tab,
    // then Log, Breakpoints, Memory Map, etc.
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->tabBar()->setExpanding(false);

    m_cpuWidget       = new CPUWidget(m_debugCore, this);
    m_logView         = new LogView(this);
    m_breakpointsView = new BreakpointsView(m_debugCore, this);
    m_memoryMapView   = new MemoryMapView(m_debugCore, this);
    m_callStackView   = new CallStackView(m_debugCore, this);
    m_threadsView     = new ThreadsView(m_debugCore, this);

    m_tabWidget->addTab(m_cpuWidget,       "CPU");
    m_tabWidget->addTab(m_logView,         "Log");
    m_tabWidget->addTab(m_breakpointsView, "Breakpoints");
    m_tabWidget->addTab(m_memoryMapView,   "Memory Map");
    m_tabWidget->addTab(m_callStackView,   "Call Stack");
    m_tabWidget->addTab(m_threadsView,     "Threads");

    // CPU tab selected by default
    m_tabWidget->setCurrentIndex(0);

    setCentralWidget(m_tabWidget);
}

void MainWindow::createCommandBar()
{
    m_commandLine = new CommandLineEdit(m_debugCore, this);
    m_commandLine->setDumpWidget(m_cpuWidget->getDump());
    m_commandLine->setLogView(m_logView);

    // Route command output to the log view
    connect(m_commandLine, &CommandLineEdit::commandOutput,
            m_logView, &LogView::addMessage);

    // Add to status bar area (bottom of window)
    statusBar()->addPermanentWidget(m_commandLine, 1);
}

void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel("Initialized");
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::connectSignals()
{
    // Debug state → status bar
    connect(m_debugCore, &DebugCore::processStateChanged,
            this, &MainWindow::updateStatusLabel);

    // Debug output → log
    connect(m_debugCore, &DebugCore::outputReceived,
            m_logView, &LogView::addMessage);

    // Register/memory changes → refresh all CPU widgets
    connect(m_debugCore, &DebugCore::registersChanged,
            m_cpuWidget->getRegisters(), &CPURegistersView::refresh);
    connect(m_debugCore, &DebugCore::registersChanged,
            m_cpuWidget->getDisassembly(), &CPUDisassembly::refresh);
    connect(m_debugCore, &DebugCore::registersChanged,
            m_cpuWidget->getSideBar(), &CPUSideBar::refresh);
    connect(m_debugCore, &DebugCore::registersChanged,
            m_cpuWidget->getArguments(), &CPUArgumentWidget::refresh);
    connect(m_debugCore, &DebugCore::breakpointsChanged,
            m_cpuWidget->getSideBar(), &CPUSideBar::refresh);
    connect(m_debugCore, &DebugCore::memoryChanged,
            m_cpuWidget->getStack(), &CPUStack::refresh);
    connect(m_debugCore, &DebugCore::memoryChanged,
            m_cpuWidget->getDump(), &CPUDump::refresh);
    connect(m_debugCore, &DebugCore::breakpointsChanged,
            m_breakpointsView, &BreakpointsView::refresh);
    // Disassembly already refreshes via registersChanged (which fires
    // alongside breakpointsChanged in emitAllRefresh), so no separate
    // connection needed here — avoids redundant re-disassembly.
    connect(m_debugCore, &DebugCore::threadListChanged,
            m_threadsView, &ThreadsView::refresh);
    connect(m_debugCore, &DebugCore::modulesChanged,
            m_memoryMapView, &MemoryMapView::refresh);

    // Disassembly selection → InfoBox update
    connect(m_cpuWidget->getDisassembly(), &CPUDisassembly::addressSelected,
            m_cpuWidget->getInfoBox(), &CPUInfoBox::updateInfo);
}

void MainWindow::updateStatusLabel(int state)
{
    QString text;
    QString color;

    switch (static_cast<DebugCore::ProcessState>(state)) {
    case DebugCore::Unloaded:
        text = "Initialized";
        color = "#8B8B9A";
        break;
    case DebugCore::Stopped:
        text = "Paused";
        color = "#F0822D";
        break;
    case DebugCore::Running:
        text = "Running";
        color = "#167C3D";
        break;
    case DebugCore::Exited:
        text = "Terminated";
        color = "#E53E3E";
        break;
    case DebugCore::Crashed:
        text = "Crashed";
        color = "#E53E3E";
        break;
    }

    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QString("QLabel { color: %1; font-weight: 600; font-size: 12px; }").arg(color));
}

void MainWindow::applyTheme()
{
    QColor bg       = ConfigColor("DisassemblyBackgroundColor");
    QColor fg       = ConfigColor("DisassemblyTextColor");
    QColor selBg    = ConfigColor("DisassemblySelectionColor");
    QColor chrome   = ConfigColor("ChromeBackgroundColor");
    QColor surface  = ConfigColor("ChromeSurfaceColor");
    QColor border   = ConfigColor("ChromeBorderColor");
    QColor hover    = ConfigColor("ChromeHoverColor");
    QColor accent   = ConfigColor("ChromeAccentColor");
    QColor muted    = ConfigColor("ChromeMutedTextColor");

    // Scrollbar handle hover — slightly lighter/darker than border
    QString scrollHover = Config()->isDarkTheme() ? "#4f5256" : "#C0C0CA";
    QString pressedBg   = Config()->isDarkTheme() ? "#4f5256" : "#D8D8DE";

    setStyleSheet(QString(
        "QMainWindow { background-color: %1; }"

        "QMenuBar { background-color: %6; color: %2; border: none; padding: 2px 0; font-size: 13px; }"
        "QMenuBar::item { padding: 4px 10px; border-radius: 4px; margin: 1px 2px; }"
        "QMenuBar::item:selected { background-color: %5; }"
        "QMenu { background-color: %7; color: %2; border: 1px solid %8; border-radius: 6px; padding: 4px 0; }"
        "QMenu::item { padding: 5px 24px 5px 12px; border-radius: 4px; margin: 1px 4px; }"
        "QMenu::item:selected { background-color: %3; }"
        "QMenu::separator { height: 1px; background: %8; margin: 4px 8px; }"

        "QStatusBar { background-color: %6; color: %9; border-top: 1px solid %8; font-size: 12px; padding: 2px 8px; }"

        "QTabWidget::pane { border: none; background-color: %1; }"
        "QTabWidget::tab-bar { left: 0; alignment: left; }"
        "QTabBar { background-color: %6; }"
        "QTabBar::tab { background-color: transparent; color: %9; padding: 8px 16px; border: none;"
        "  border-bottom: 2px solid transparent; font-size: 13px; font-weight: 500; }"
        "QTabBar::tab:selected { color: %4; border-bottom: 2px solid %4; }"
        "QTabBar::tab:hover:!selected { color: %2; border-bottom: 2px solid %8; }"

        "QSplitter::handle { background-color: %8; }"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }"

        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %8; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: %10; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: %8; border-radius: 4px; min-width: 24px; }"
        "QScrollBar::handle:horizontal:hover { background: %10; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"

        "QToolTip { background-color: %7; color: %2; border: 1px solid %8; border-radius: 4px;"
        "  padding: 4px 8px; font-size: 12px; }"

    ).arg(
        bg.name(),        // %1
        fg.name(),        // %2
        selBg.name(),     // %3
        accent.name(),    // %4
        hover.name(),     // %5
        chrome.name(),    // %6
        surface.name(),   // %7
        border.name(),    // %8
        muted.name()      // %9
    ).arg(
        scrollHover       // %10
    ));

    // Toolbar
    m_toolBar->setStyleSheet(QString(
        "QToolBar { background-color: %1; border: none; spacing: 2px;"
        "  border-bottom: 1px solid %2; padding: 3px 6px; }"
        "QToolButton { background: transparent; border: 1px solid transparent;"
        "  border-radius: 5px; padding: 4px; }"
        "QToolButton:hover { background-color: %3; }"
        "QToolButton:pressed { background-color: %4; }"
    ).arg(chrome.name(), border.name(), hover.name(), pressedBg));

    // Refresh all child widget styles
    m_cpuWidget->getDisassembly()->style()->unpolish(m_cpuWidget->getDisassembly());
    m_cpuWidget->getDisassembly()->style()->polish(m_cpuWidget->getDisassembly());
    m_cpuWidget->getRegisters()->style()->unpolish(m_cpuWidget->getRegisters());
    m_cpuWidget->getRegisters()->style()->polish(m_cpuWidget->getRegisters());
    m_cpuWidget->getStack()->style()->unpolish(m_cpuWidget->getStack());
    m_cpuWidget->getStack()->style()->polish(m_cpuWidget->getStack());
    m_cpuWidget->getDump()->style()->unpolish(m_cpuWidget->getDump());
    m_cpuWidget->getDump()->style()->polish(m_cpuWidget->getDump());
}
