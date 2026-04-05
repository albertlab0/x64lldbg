# x64lldbg

A cross-platform GUI debugger frontend for LLDB, inspired by x64dbg's UI and workflow.

## Project Overview

- **Name**: x64lldbg
- **Language**: C++17
- **GUI Framework**: Qt 6 (Qt 5 fallback if needed)
- **Debug Backend**: LLDB C++ API (SB API: SBDebugger, SBTarget, SBProcess, SBThread, SBFrame, etc.)
- **Build System**: CMake
- **Target Platforms**: macOS, Linux

## Architecture

### Three-Layer Design

1. **UI Layer** (Qt widgets) — x64dbg-inspired layout with dockable panels
2. **Debug Abstraction Layer** — C++ wrapper around LLDB's SB API, exposing clean signals/slots
3. **LLDB Backend** — Direct use of liblldb via the stable C++ SB API

### Key UI Panels (mirroring x64dbg's CPUWidget layout)

- **CPUDisassembly** — Main disassembly view with syntax highlighting
- **CPUInfoBox** — The "middle pane" below disassembly showing memory/register info for the selected instruction
- **CPURegistersView** — Register display and editing
- **CPUStack** — Stack view
- **CPUDump** — Memory hex dump
- **CPUSideBar** — Breakpoint markers, jump arrows
- **BreakpointsView** — Breakpoint management (software, hardware, conditional)
- **MemoryMapView** — Process memory layout
- **CallStackView** — Call stack
- **ThreadsView** — Thread listing
- **LogView** — Debug output/log
- **ScriptView** — Python scripting console (via LLDB's built-in Python support)

### Theming

- Color scheme system supporting multiple themes
- Ships with: **x64dbg Default** (dark) and **Cutter** theme
- Themes defined as JSON or INI color maps, applied via Qt stylesheets + custom painting
- Per-widget color configuration (disassembly, registers, stack, hex dump, etc.)

### Python Scripting

- Leverages LLDB's built-in Python scripting support (`lldb` Python module)
- Embedded Python interpreter for user scripts
- Script console widget in the UI

## Debugger Features

- Software breakpoints (int3 / trap)
- Hardware breakpoints (debug registers)
- Conditional breakpoints (expression-based)
- Step into, step over, step out, run to cursor
- Register read/write
- Memory read/write
- Process attach/detach
- Multi-thread debugging

### Smart Pointer Dereferencing

When a register or memory value contains a pointer, automatically dereference and display:
1. **String content** — If pointer targets a valid ASCII/Unicode string, show the string inline (e.g., `RSI = 0x7fff5a2b → "Hello, World!"`)
2. **Symbol/label** — If pointer targets a known symbol, show `<module.symbol>` (e.g., `RAX = 0x1000 <libc.malloc>`)
3. **Module + offset** — If pointer is within a known module, show `module+0xoffset`
4. **Nested dereference** — Follow pointer chains where useful

This applies to:
- **CPURegistersView** — Every register value that looks like a valid address gets dereferenced
- **CPUInfoBox** — Instruction operands are resolved with full symbolic + string info
- **CPUStack** — Stack entries that are pointers show dereferenced info
- **CPUDump** — Pointer values in hex dump show dereferenced tooltips/annotations

Reference: `ref/x64dbg/src/gui/Src/Gui/RegistersView.cpp` lines 1910-1960 (`DbgGetStringAt`)

## Keyboard Shortcuts (x64dbg-compatible)

### Debug Control
| Key | Action |
|-----|--------|
| F9 | Run / Continue |
| F7 | Step Into |
| F8 | Step Over |
| Ctrl+F9 | Step Out (Execute Till Return) |
| F4 | Run to Cursor (Run to Selection) |
| Shift+F4 | Run to Expression |
| F12 | Pause |
| Ctrl+F2 | Restart |
| Alt+F2 | Stop / Close debuggee |

### Breakpoints
| Key | Action |
|-----|--------|
| F2 | Toggle Breakpoint |
| Shift+F2 | Set/Edit Conditional Breakpoint |
| Delete | Delete Breakpoint (in breakpoint views) |
| Space | Enable/Disable Breakpoint |

### Stepping (Advanced)
| Key | Action |
|-----|--------|
| Ctrl+F7 | Animate Into (auto-step with visual) |
| Ctrl+F8 | Animate Over (auto-step with visual) |
| Ctrl+Alt+F7 | Trace Into (conditional) |
| Ctrl+Alt+F8 | Trace Over (conditional) |
| Alt+F9 | Run to User Code |

### Navigation
| Key | Action |
|-----|--------|
| \* | Go to Origin (current IP) |
| Ctrl+G | Go to Address |
| Ctrl+\* | Set IP Here (move instruction pointer) |

### Annotations
| Key | Action |
|-----|--------|
| ; | Set Comment |
| : | Set Label |
| Ctrl+D | Toggle Bookmark |

Shortcuts should be user-configurable via a Shortcuts dialog (like x64dbg's `ShortcutsDialog`).

Reference: `ref/x64dbg/src/gui/Src/Utils/Configuration.cpp` (line 415+) for full default shortcut map

## Directory Structure

```
x64lldbg/
  CMakeLists.txt
  src/
    main.cpp
    core/           # Debug abstraction layer over LLDB
    gui/            # Qt widgets and UI
      widgets/      # Individual panel widgets
      dialogs/      # Dialogs (attach, settings, breakpoint config)
      themes/       # Theme files (stylesheets, color maps)
    common/         # Shared utilities, configuration
  resources/        # Icons, assets
  ref/              # Reference code (x64dbg, cutter) - not part of build
```

## Build & Run

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
./x64lldbg
```

### Dependencies

- Qt 6 (Widgets, Core, Gui)
- LLDB (liblldb, lldb development headers)
- Python 3 (for scripting support, typically bundled with LLDB)
- CMake 3.20+

## Reference Code

- `ref/x64dbg/` — x64dbg source, primary UI reference
  - `src/gui/Src/Gui/CPUWidget.*` — Main CPU view layout
  - `src/gui/Src/Gui/CPUInfoBox.*` — The info box (middle pane)
  - `src/gui/Src/Gui/CPUDisassembly.*` — Disassembly widget
  - `src/gui/Src/Utils/Configuration.*` — Theme/color system
- `ref/cutter/` — Cutter source, theme reference
  - `src/themes/` — QSS stylesheets for dark/light/midnight themes
  - `src/common/Configuration.*` — Theme loading mechanism
  - `src/common/ColorThemeWorker.*` — Color theme management

## Code Style

- C++17 standard
- CamelCase for class names, camelCase for methods/variables
- Qt naming conventions (signals/slots, Q_OBJECT macro)
- Header guards or `#pragma once`
