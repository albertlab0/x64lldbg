#include "Configuration.h"
#include <QFontInfo>
#include <QFontDatabase>

Configuration* Configuration::s_instance = nullptr;

Configuration* Configuration::instance()
{
    if (!s_instance)
        s_instance = new Configuration();
    return s_instance;
}

Configuration::Configuration()
    : QObject(nullptr)
{
    loadDefaults();
}

void Configuration::loadDefaults()
{
    loadX64dbgDefaultColors();
    loadDefaultShortcuts();
    loadDefaultFonts();
}

void Configuration::setTheme(Theme theme)
{
    if (m_theme == theme) return;
    m_theme = theme;

    switch (theme) {
    case X64dbgDefault: loadX64dbgDefaultColors(); break;
    case ModernLight:   loadModernLightColors();   break;
    case CutterDark:    loadCutterDarkColors();    break;
    }

    emit colorsUpdated();
    emit themeChanged();
}

// ============================================================
//  x64dbg Default — classic cream background, x64dbg colors
//  with modern flat chrome
// ============================================================
void Configuration::loadX64dbgDefaultColors()
{
    // Disassembly — x64dbg cream/off-white
    m_colors["DisassemblyBackgroundColor"]         = QColor("#FFF8F0");
    m_colors["DisassemblySelectionColor"]          = QColor("#C0C0C0");
    m_colors["DisassemblyTextColor"]               = QColor("#000000");
    m_colors["DisassemblyAddressColor"]            = QColor("#808080");
    m_colors["DisassemblyBytesColor"]              = QColor("#808080");
    m_colors["DisassemblyMnemonicColor"]           = QColor("#000000");
    m_colors["DisassemblyRegistersColor"]          = QColor("#008300");
    m_colors["DisassemblyNumbersColor"]            = QColor("#828200");
    m_colors["DisassemblyMemoryColor"]             = QColor("#000000");
    m_colors["DisassemblyCommentColor"]            = QColor("#008300");
    m_colors["DisassemblyLabelColor"]              = QColor("#FF0000");
    m_colors["DisassemblyJumpArrowColor"]          = QColor("#000000");
    m_colors["DisassemblyConditionalJumpColor"]    = QColor("#FF0000");  // red for jcc
    m_colors["DisassemblyUnconditionalJumpColor"]  = QColor("#FF0000");  // red for jmp
    m_colors["DisassemblyCallColor"]               = QColor("#00868B");  // dark cyan for call
    m_colors["DisassemblyRetColor"]                = QColor("#00868B");  // dark cyan for ret
    m_colors["DisassemblyNopColor"]                = QColor("#808080");
    m_colors["DisassemblyPushPopColor"]            = QColor("#0000FF");
    m_colors["DisassemblyAddressOperandColor"]     = QColor("#828200");  // yellow/gold for address operands

    // x64dbg-style instruction-type row backgrounds (disassembly columns only)
    m_colors["DisassemblyCallBgColor"]             = QColor("#00FFFF");  // cyan for call/ret
    m_colors["DisassemblyRetBgColor"]              = QColor("#00FFFF");  // cyan for ret
    m_colors["DisassemblyConditionalJumpBgColor"]  = QColor("#FFFF00");  // yellow for jcc/jmp

    m_colors["BreakpointBackgroundColor"]          = QColor("#FF0000");
    m_colors["BreakpointColor"]                    = QColor("#FF0000");
    m_colors["HardwareBreakpointColor"]            = QColor("#FF8000");
    m_colors["BookmarkColor"]                      = QColor("#FEC000");

    // CIP — white on black (classic x64dbg)
    m_colors["CurrentIPBackgroundColor"]           = QColor("#000000");
    m_colors["CurrentIPColor"]                     = QColor("#FFFFFF");

    m_colors["RegistersBackgroundColor"]           = QColor("#FFF8F0");
    m_colors["RegistersTextColor"]                 = QColor("#000000");
    m_colors["RegistersModifiedColor"]             = QColor("#FF0000");
    m_colors["RegistersLabelColor"]                = QColor("#008300");

    m_colors["StackBackgroundColor"]               = QColor("#FFF8F0");
    m_colors["StackTextColor"]                     = QColor("#000000");
    m_colors["StackAddressColor"]                  = QColor("#808080");
    m_colors["StackCurrentSPColor"]                = QColor("#000000");

    m_colors["HexDumpBackgroundColor"]             = QColor("#FFF8F0");
    m_colors["HexDumpTextColor"]                   = QColor("#000000");
    m_colors["HexDumpAddressColor"]                = QColor("#808080");
    m_colors["HexDumpModifiedColor"]               = QColor("#FF0000");
    m_colors["HexDumpAsciiColor"]                  = QColor("#000000");

    m_colors["InfoBoxBackgroundColor"]             = QColor("#FFF8F0");
    m_colors["InfoBoxTextColor"]                   = QColor("#000000");

    m_colors["SideBarBackgroundColor"]             = QColor("#FFF8F0");
    m_colors["SideBarBulletColor"]                 = QColor("#808080");
    m_colors["SideBarJumpLineColor"]               = QColor("#C0C0C0");
    m_colors["SideBarJumpSelectedColor"]           = QColor("#FF0000");

    m_colors["TableHeaderBackgroundColor"]         = QColor("#C0C0C0");
    m_colors["TableHeaderTextColor"]               = QColor("#000000");
    m_colors["TableGridColor"]                     = QColor("#C0C0C0");
    m_colors["AlternateRowColor"]                  = QColor("#FFF0E0");

    m_colors["StringReferenceColor"]               = QColor("#008300");
    m_colors["SymbolReferenceColor"]               = QColor("#0000FF");
    m_colors["ModuleReferenceColor"]               = QColor("#808080");

    // Chrome — modern flat UI wrapping classic x64dbg content colors
    m_colors["ChromeBackgroundColor"]              = QColor("#F0F0F0");
    m_colors["ChromeSurfaceColor"]                 = QColor("#FFFFFF");
    m_colors["ChromeBorderColor"]                  = QColor("#D0D0D0");
    m_colors["ChromeHoverColor"]                   = QColor("#E0E0E0");
    m_colors["ChromeAccentColor"]                  = QColor("#0078D7");
    m_colors["ChromeMutedTextColor"]               = QColor("#666666");
}

// ============================================================
//  Modern Light — refined light theme with soft blue accents
// ============================================================
void Configuration::loadModernLightColors()
{
    // Disassembly — clean white with soft tones
    m_colors["DisassemblyBackgroundColor"]         = QColor("#FBFBFB");
    m_colors["DisassemblySelectionColor"]          = QColor("#E2EFFA");
    m_colors["DisassemblyTextColor"]               = QColor("#1A1A2E");
    m_colors["DisassemblyAddressColor"]            = QColor("#8B8B9A");
    m_colors["DisassemblyBytesColor"]              = QColor("#8B8B9A");
    m_colors["DisassemblyMnemonicColor"]           = QColor("#1A1A2E");
    m_colors["DisassemblyRegistersColor"]          = QColor("#167C3D");
    m_colors["DisassemblyNumbersColor"]            = QColor("#9B6700");
    m_colors["DisassemblyMemoryColor"]             = QColor("#1A1A2E");
    m_colors["DisassemblyCommentColor"]            = QColor("#6B7280");
    m_colors["DisassemblyLabelColor"]              = QColor("#1D6FB5");
    m_colors["DisassemblyJumpArrowColor"]          = QColor("#1A1A2E");
    m_colors["DisassemblyConditionalJumpColor"]    = QColor("#C03030");
    m_colors["DisassemblyUnconditionalJumpColor"]  = QColor("#C03030");
    m_colors["DisassemblyCallColor"]               = QColor("#1D6FB5");
    m_colors["DisassemblyRetColor"]                = QColor("#1D6FB5");
    m_colors["DisassemblyNopColor"]                = QColor("#8B8B9A");
    m_colors["DisassemblyPushPopColor"]            = QColor("#1D6FB5");
    m_colors["DisassemblyAddressOperandColor"]     = QColor("#9B6700");

    // Instruction-type row backgrounds (disassembly columns only)
    m_colors["DisassemblyCallBgColor"]             = QColor("#E0F7FA");  // soft cyan
    m_colors["DisassemblyRetBgColor"]              = QColor("#E0F7FA");  // soft cyan
    m_colors["DisassemblyConditionalJumpBgColor"]  = QColor("#FFF9C4");  // soft yellow

    m_colors["BreakpointBackgroundColor"]          = QColor("#FFE0E0");
    m_colors["BreakpointColor"]                    = QColor("#E53E3E");
    m_colors["HardwareBreakpointColor"]            = QColor("#F0822D");
    m_colors["BookmarkColor"]                      = QColor("#F0C040");

    // CIP — accent blue with white text
    m_colors["CurrentIPBackgroundColor"]           = QColor("#1D6FB5");
    m_colors["CurrentIPColor"]                     = QColor("#FFFFFF");

    m_colors["RegistersBackgroundColor"]           = QColor("#FBFBFB");
    m_colors["RegistersTextColor"]                 = QColor("#1A1A2E");
    m_colors["RegistersModifiedColor"]             = QColor("#E53E3E");
    m_colors["RegistersLabelColor"]                = QColor("#167C3D");

    m_colors["StackBackgroundColor"]               = QColor("#FBFBFB");
    m_colors["StackTextColor"]                     = QColor("#1A1A2E");
    m_colors["StackAddressColor"]                  = QColor("#8B8B9A");
    m_colors["StackCurrentSPColor"]                = QColor("#1D6FB5");

    m_colors["HexDumpBackgroundColor"]             = QColor("#FBFBFB");
    m_colors["HexDumpTextColor"]                   = QColor("#1A1A2E");
    m_colors["HexDumpAddressColor"]                = QColor("#8B8B9A");
    m_colors["HexDumpModifiedColor"]               = QColor("#E53E3E");
    m_colors["HexDumpAsciiColor"]                  = QColor("#1A1A2E");

    m_colors["InfoBoxBackgroundColor"]             = QColor("#FBFBFB");
    m_colors["InfoBoxTextColor"]                   = QColor("#1A1A2E");

    m_colors["SideBarBackgroundColor"]             = QColor("#FBFBFB");
    m_colors["SideBarBulletColor"]                 = QColor("#C0C0CA");
    m_colors["SideBarJumpLineColor"]               = QColor("#D0D0DA");
    m_colors["SideBarJumpSelectedColor"]           = QColor("#C03030");

    m_colors["TableHeaderBackgroundColor"]         = QColor("#F0F0F2");
    m_colors["TableHeaderTextColor"]               = QColor("#1A1A2E");
    m_colors["TableGridColor"]                     = QColor("#E8E8EC");
    m_colors["AlternateRowColor"]                  = QColor("#F5F5F8");

    m_colors["StringReferenceColor"]               = QColor("#167C3D");
    m_colors["SymbolReferenceColor"]               = QColor("#1D6FB5");
    m_colors["ModuleReferenceColor"]               = QColor("#8B8B9A");

    // Chrome — clean white with subtle borders
    m_colors["ChromeBackgroundColor"]              = QColor("#F5F5F7");
    m_colors["ChromeSurfaceColor"]                 = QColor("#FFFFFF");
    m_colors["ChromeBorderColor"]                  = QColor("#E0E0E5");
    m_colors["ChromeHoverColor"]                   = QColor("#EAEAEF");
    m_colors["ChromeAccentColor"]                  = QColor("#1D6FB5");
    m_colors["ChromeMutedTextColor"]               = QColor("#8B8B9A");
}

// ============================================================
//  Cutter Dark — inspired by Cutter's midnight theme
// ============================================================
void Configuration::loadCutterDarkColors()
{
    // Disassembly — Cutter midnight palette
    m_colors["DisassemblyBackgroundColor"]         = QColor("#25282b");
    m_colors["DisassemblySelectionColor"]          = QColor("#36393c");
    m_colors["DisassemblyTextColor"]               = QColor("#eff0f1");
    m_colors["DisassemblyAddressColor"]            = QColor("#6c7680");
    m_colors["DisassemblyBytesColor"]              = QColor("#555d66");
    m_colors["DisassemblyMnemonicColor"]           = QColor("#eff0f1");
    m_colors["DisassemblyRegistersColor"]          = QColor("#21d893");  // Cutter green
    m_colors["DisassemblyNumbersColor"]            = QColor("#dda368");  // Cutter orange/symbol
    m_colors["DisassemblyMemoryColor"]             = QColor("#eff0f1");
    m_colors["DisassemblyCommentColor"]            = QColor("#6c7680");
    m_colors["DisassemblyLabelColor"]              = QColor("#328cff");  // Cutter blue
    m_colors["DisassemblyJumpArrowColor"]          = QColor("#eff0f1");
    m_colors["DisassemblyConditionalJumpColor"]    = QColor("#e95656");  // Cutter red
    m_colors["DisassemblyUnconditionalJumpColor"]  = QColor("#e95656");  // Cutter red
    m_colors["DisassemblyCallColor"]               = QColor("#42eef4");  // Cutter cyan
    m_colors["DisassemblyRetColor"]                = QColor("#42eef4");  // Cutter cyan
    m_colors["DisassemblyNopColor"]                = QColor("#555d66");
    m_colors["DisassemblyPushPopColor"]            = QColor("#328cff");  // Cutter blue
    m_colors["DisassemblyAddressOperandColor"]     = QColor("#dda368");  // Cutter orange for addresses

    // Instruction-type row backgrounds (disassembly columns only)
    m_colors["DisassemblyCallBgColor"]             = QColor("#1a3a3c");  // dark cyan tint
    m_colors["DisassemblyRetBgColor"]              = QColor("#1a3a3c");  // dark cyan tint
    m_colors["DisassemblyConditionalJumpBgColor"]  = QColor("#3a3520");  // dark yellow tint

    // Breakpoints
    m_colors["BreakpointBackgroundColor"]          = QColor("#8c4c4c");
    m_colors["BreakpointColor"]                    = QColor("#e95656");
    m_colors["HardwareBreakpointColor"]            = QColor("#dda368");
    m_colors["BookmarkColor"]                      = QColor("#dda368");

    // Current IP — Cutter-style PC highlight
    m_colors["CurrentIPBackgroundColor"]           = QColor("#42eef4");  // cyan
    m_colors["CurrentIPColor"]                     = QColor("#1f2022");  // dark text

    // Registers
    m_colors["RegistersBackgroundColor"]           = QColor("#25282b");
    m_colors["RegistersTextColor"]                 = QColor("#eff0f1");
    m_colors["RegistersModifiedColor"]             = QColor("#e95656");
    m_colors["RegistersLabelColor"]                = QColor("#21d893");

    // Stack
    m_colors["StackBackgroundColor"]               = QColor("#25282b");
    m_colors["StackTextColor"]                     = QColor("#eff0f1");
    m_colors["StackAddressColor"]                  = QColor("#6c7680");
    m_colors["StackCurrentSPColor"]                = QColor("#42eef4");

    // Hex dump
    m_colors["HexDumpBackgroundColor"]             = QColor("#25282b");
    m_colors["HexDumpTextColor"]                   = QColor("#eff0f1");
    m_colors["HexDumpAddressColor"]                = QColor("#6c7680");
    m_colors["HexDumpModifiedColor"]               = QColor("#e95656");
    m_colors["HexDumpAsciiColor"]                  = QColor("#82c86f");  // Cutter green

    // InfoBox
    m_colors["InfoBoxBackgroundColor"]             = QColor("#1f2022");
    m_colors["InfoBoxTextColor"]                   = QColor("#eff0f1");

    // Sidebar
    m_colors["SideBarBackgroundColor"]             = QColor("#1f2022");
    m_colors["SideBarBulletColor"]                 = QColor("#4f5256");
    m_colors["SideBarJumpLineColor"]               = QColor("#4f5256");
    m_colors["SideBarJumpSelectedColor"]           = QColor("#e95656");

    // General chrome
    m_colors["TableHeaderBackgroundColor"]         = QColor("#2b2f32");
    m_colors["TableHeaderTextColor"]               = QColor("#eff0f1");
    m_colors["TableGridColor"]                     = QColor("#2a2b2f");
    m_colors["AlternateRowColor"]                  = QColor("#1c1f24");

    // String dereference display
    m_colors["StringReferenceColor"]               = QColor("#82c86f");
    m_colors["SymbolReferenceColor"]               = QColor("#328cff");
    m_colors["ModuleReferenceColor"]               = QColor("#6c7680");

    // Chrome colors
    m_colors["ChromeBackgroundColor"]              = QColor("#1f2022");
    m_colors["ChromeSurfaceColor"]                 = QColor("#232629");
    m_colors["ChromeBorderColor"]                  = QColor("#2a2b2f");
    m_colors["ChromeHoverColor"]                   = QColor("#36393c");
    m_colors["ChromeAccentColor"]                  = QColor("#3daee9");  // Cutter blue accent
    m_colors["ChromeMutedTextColor"]               = QColor("#76797C");
}

// ----- Shortcuts (x64dbg defaults) -----
// On macOS Qt maps Qt::CTRL to ⌘ (Command). We want the physical Ctrl key
// for x64dbg-compatible shortcuts, which is Qt::META on macOS.
#ifdef Q_OS_MACOS
static constexpr auto PHYS_CTRL = Qt::META;
#else
static constexpr auto PHYS_CTRL = Qt::CTRL;
#endif

void Configuration::loadDefaultShortcuts()
{
    m_shortcuts["DebugRun"]              = {"Run",                   QKeySequence(Qt::Key_F9)};
    m_shortcuts["DebugStepInto"]         = {"Step Into",             QKeySequence(Qt::Key_F7)};
    m_shortcuts["DebugStepOver"]         = {"Step Over",             QKeySequence(Qt::Key_F8)};
    m_shortcuts["DebugStepOut"]          = {"Step Out",              QKeySequence(PHYS_CTRL | Qt::Key_F9)};
    m_shortcuts["DebugRunToCursor"]      = {"Run to Cursor",         QKeySequence(Qt::Key_F4)};
    m_shortcuts["DebugRunToExpression"]  = {"Run to Expression",     QKeySequence(Qt::SHIFT | Qt::Key_F4)};
    m_shortcuts["DebugPause"]            = {"Pause",                 QKeySequence(Qt::Key_F12)};
    m_shortcuts["DebugRestart"]          = {"Restart",               QKeySequence(PHYS_CTRL | Qt::Key_F2)};
    m_shortcuts["DebugClose"]            = {"Stop",                  QKeySequence(Qt::ALT | Qt::Key_F2)};

    m_shortcuts["ToggleBreakpoint"]      = {"Toggle Breakpoint",     QKeySequence(Qt::Key_F2)};
    m_shortcuts["EditBreakpoint"]        = {"Conditional Breakpoint", QKeySequence(Qt::SHIFT | Qt::Key_F2)};

    m_shortcuts["DebugAnimateInto"]      = {"Animate Into",          QKeySequence(PHYS_CTRL | Qt::Key_F7)};
    m_shortcuts["DebugAnimateOver"]      = {"Animate Over",          QKeySequence(PHYS_CTRL | Qt::Key_F8)};
    m_shortcuts["DebugTraceInto"]        = {"Trace Into",            QKeySequence(PHYS_CTRL | Qt::ALT | Qt::Key_F7)};
    m_shortcuts["DebugTraceOver"]        = {"Trace Over",            QKeySequence(PHYS_CTRL | Qt::ALT | Qt::Key_F8)};
    m_shortcuts["DebugRunToUserCode"]    = {"Run to User Code",      QKeySequence(Qt::ALT | Qt::Key_F9)};

    m_shortcuts["GotoOrigin"]            = {"Go to Origin",          QKeySequence(Qt::Key_Asterisk)};
    m_shortcuts["GotoAddress"]           = {"Go to Address",         QKeySequence(PHYS_CTRL | Qt::Key_G)};
    m_shortcuts["SetIPHere"]             = {"Set IP Here",           QKeySequence(PHYS_CTRL | Qt::Key_Asterisk)};

    m_shortcuts["SetComment"]            = {"Set Comment",           QKeySequence(Qt::Key_Semicolon)};
    m_shortcuts["SetLabel"]              = {"Set Label",             QKeySequence(Qt::Key_Colon)};
    m_shortcuts["ToggleBookmark"]        = {"Toggle Bookmark",       QKeySequence(PHYS_CTRL | Qt::Key_D)};
}

// ----- Fonts -----
void Configuration::loadDefaultFonts()
{
#ifdef Q_OS_MACOS
    // Use hasFamily() to avoid expensive font alias population for missing fonts
    QFont mono;
    if (QFontDatabase::families().contains("SF Mono"))
        mono = QFont("SF Mono", 12);
    else
        mono = QFont("Menlo", 12);
#else
    QFont mono;
    if (QFontDatabase::families().contains("JetBrains Mono"))
        mono = QFont("JetBrains Mono", 10);
    else if (QFontDatabase::families().contains("Fira Code"))
        mono = QFont("Fira Code", 10);
    else if (QFontDatabase::families().contains("Cascadia Mono"))
        mono = QFont("Cascadia Mono", 10);
    else
        mono = QFont("Monospace", 10);
#endif
    mono.setStyleHint(QFont::Monospace);

    m_fonts["Disassembly"]  = mono;
    m_fonts["Registers"]    = mono;
    m_fonts["Stack"]        = mono;
    m_fonts["HexDump"]      = mono;
    m_fonts["InfoBox"]      = mono;
    m_fonts["Log"]          = mono;
    m_fonts["Default"]      = mono;
}

// ----- Accessors -----
QColor Configuration::getColor(const QString& id) const
{
    return m_colors.value(id, QColor(0xD4, 0xD4, 0xD4));
}

void Configuration::setColor(const QString& id, const QColor& color)
{
    m_colors[id] = color;
    emit colorsUpdated();
}

QFont Configuration::getFont(const QString& id) const
{
    return m_fonts.value(id, m_fonts.value("Default"));
}

Shortcut Configuration::getShortcut(const QString& id) const
{
    return m_shortcuts.value(id, Shortcut{"Unknown", QKeySequence()});
}
