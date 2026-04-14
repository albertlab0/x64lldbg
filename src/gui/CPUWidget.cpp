#include "CPUWidget.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTimer>
#include <QScrollBar>

#include "core/DebugCore.h"
#include "gui/widgets/CPUDisassembly.h"
#include "gui/widgets/CPUInfoBox.h"
#include "gui/widgets/CPUSideBar.h"
#include "gui/widgets/CPURegistersView.h"
#include "gui/widgets/CPUStack.h"
#include "gui/widgets/CPUDump.h"
#include "gui/widgets/CPUArgumentWidget.h"

CPUWidget::CPUWidget(DebugCore* debugCore, QWidget* parent)
    : QWidget(parent)
    , m_debugCore(debugCore)
{
    // Create child widgets
    m_sideBar     = new CPUSideBar(debugCore, this);
    m_disassembly = new CPUDisassembly(debugCore, this);
    m_infoBox     = new CPUInfoBox(debugCore, this);
    m_registers   = new CPURegistersView(debugCore, this);
    m_arguments   = new CPUArgumentWidget(debugCore, this);
    m_dump        = new CPUDump(debugCore, this);
    m_stack       = new CPUStack(debugCore, this);

    setupLayout();

    // Sync sidebar with disassembly table
    m_sideBar->setRowHeight(m_disassembly->verticalHeader()->defaultSectionSize());
    m_sideBar->setTableWidget(m_disassembly);

    // Repaint sidebar when disassembly scrolls (keeps jump arrows aligned)
    connect(m_disassembly->verticalScrollBar(), &QScrollBar::valueChanged,
            m_sideBar, QOverload<>::of(&QWidget::update));

    // Feed disassembly lines to sidebar so it shows correct IP/breakpoint markers
    connect(m_disassembly, &CPUDisassembly::linesChanged, this, [this]() {
        m_sideBar->setLines(m_disassembly->getLines());
    });

    // Highlight selected instruction's jump arrow in the sidebar
    connect(m_disassembly, &CPUDisassembly::addressSelected,
            m_sideBar, &CPUSideBar::setSelectedAddress);
}

void CPUWidget::setupLayout()
{
    // Build the splitter tree matching x64dbg's CPUWidget layout:
    //
    // mVSplitter (Vertical)
    // +-- mTopHSplitter (Horizontal)
    // |   +-- mTopLeftVSplitter (Vertical)
    // |   |   +-- mTopLeftUpperHSplitter (Horizontal)
    // |   |   |   +-- CPUSideBar
    // |   |   |   +-- CPUDisassembly
    // |   |   +-- CPUInfoBox
    // |   +-- mTopRightVSplitter (Vertical)
    // |       +-- CPURegistersView
    // |       +-- CPUArgumentWidget
    // +-- mBotHSplitter (Horizontal)
    //     +-- CPUDump
    //     +-- CPUStack

    // Top-left upper: sidebar + disassembly
    m_topLeftUpperHSplitter = new QSplitter(Qt::Horizontal, this);
    m_topLeftUpperHSplitter->setHandleWidth(1);
    m_topLeftUpperHSplitter->addWidget(m_sideBar);
    m_topLeftUpperHSplitter->addWidget(m_disassembly);
    m_topLeftUpperHSplitter->setStretchFactor(0, 0);  // sidebar: fixed
    m_topLeftUpperHSplitter->setStretchFactor(1, 1);  // disassembly: stretch
    m_topLeftUpperHSplitter->setCollapsible(0, false);
    m_topLeftUpperHSplitter->setCollapsible(1, false);

    // Top-left: disassembly area + infobox
    m_topLeftVSplitter = new QSplitter(Qt::Vertical, this);
    m_topLeftVSplitter->setHandleWidth(1);
    m_topLeftVSplitter->addWidget(m_topLeftUpperHSplitter);
    m_topLeftVSplitter->addWidget(m_infoBox);
    m_topLeftVSplitter->setStretchFactor(0, 99);  // disasm area large
    m_topLeftVSplitter->setStretchFactor(1, 1);   // infobox small
    m_topLeftVSplitter->setCollapsible(0, false);
    m_topLeftVSplitter->setCollapsible(1, true);

    // Top-right: registers + arguments
    m_topRightVSplitter = new QSplitter(Qt::Vertical, this);
    m_topRightVSplitter->setHandleWidth(1);
    m_topRightVSplitter->addWidget(m_registers);
    m_topRightVSplitter->addWidget(m_arguments);
    m_topRightVSplitter->setStretchFactor(0, 87);
    m_topRightVSplitter->setStretchFactor(1, 13);
    m_topRightVSplitter->setCollapsible(0, false);
    m_topRightVSplitter->setCollapsible(1, true);

    // Top: left (disasm) + right (registers)
    m_topHSplitter = new QSplitter(Qt::Horizontal, this);
    m_topHSplitter->setHandleWidth(1);
    m_topHSplitter->addWidget(m_topLeftVSplitter);
    m_topHSplitter->addWidget(m_topRightVSplitter);
    m_topHSplitter->setStretchFactor(0, 77);
    m_topHSplitter->setStretchFactor(1, 23);
    m_topHSplitter->setCollapsible(0, false);
    m_topHSplitter->setCollapsible(1, false);

    // Bottom: dump + stack
    m_botHSplitter = new QSplitter(Qt::Horizontal, this);
    m_botHSplitter->setHandleWidth(1);
    m_botHSplitter->addWidget(m_dump);
    m_botHSplitter->addWidget(m_stack);
    m_botHSplitter->setStretchFactor(0, 77);
    m_botHSplitter->setStretchFactor(1, 23);
    m_botHSplitter->setCollapsible(0, false);
    m_botHSplitter->setCollapsible(1, false);

    // Main vertical: top + bottom
    m_vSplitter = new QSplitter(Qt::Vertical, this);
    m_vSplitter->setHandleWidth(1);
    m_vSplitter->addWidget(m_topHSplitter);
    m_vSplitter->addWidget(m_botHSplitter);
    m_vSplitter->setStretchFactor(0, 60);
    m_vSplitter->setStretchFactor(1, 40);
    m_vSplitter->setCollapsible(0, false);
    m_vSplitter->setCollapsible(1, false);

    // Sync top and bottom horizontal splitters so registers/stack widths align
    connect(m_topHSplitter, &QSplitter::splitterMoved, this, [this]() {
        m_botHSplitter->setSizes(m_topHSplitter->sizes());
    });
    connect(m_botHSplitter, &QSplitter::splitterMoved, this, [this]() {
        m_topHSplitter->setSizes(m_botHSplitter->sizes());
    });

    // Force initial sync after the first layout pass computes actual sizes
    QTimer::singleShot(0, this, [this]() {
        m_botHSplitter->setSizes(m_topHSplitter->sizes());
    });

    // Layout for this widget
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_vSplitter);
}
