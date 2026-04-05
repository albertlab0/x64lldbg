cmake_minimum_required(VERSION 3.20)
project(x64lldbg VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# --- Qt6 ---
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# --- LLDB (optional) ---
option(ENABLE_LLDB "Enable LLDB debugger backend" ON)

if(ENABLE_LLDB)
    set(HAS_LLDB FALSE)

    # Find LLDB headers (from system LLVM packages or Homebrew)
    find_path(LLDB_INCLUDE_DIR lldb/API/SBDebugger.h
        PATHS
            /usr/lib/llvm-18/include
            /usr/lib/llvm-17/include
            /usr/lib/llvm-16/include
            /usr/lib/llvm-15/include
            /usr/lib/llvm-14/include
            /usr/local/opt/llvm/include
            /opt/homebrew/opt/llvm/include
    )

    if(APPLE)
        # On macOS, use the system LLDB.framework for linking — its debugserver
        # is properly code-signed. Homebrew's liblldb ships an unsigned
        # debugserver that fails with "handshake packet timeout".
        # Headers still come from Homebrew since the system framework has none.
        find_library(LLDB_FRAMEWORK LLDB
            PATHS
                /Applications/Xcode.app/Contents/SharedFrameworks
                /Library/Developer/CommandLineTools/Library/PrivateFrameworks
            NO_DEFAULT_PATH
        )
        if(LLDB_FRAMEWORK AND LLDB_INCLUDE_DIR)
            message(STATUS "LLDB framework (link): ${LLDB_FRAMEWORK}")
            message(STATUS "LLDB headers (Homebrew): ${LLDB_INCLUDE_DIR}")
            set(LLDB_LIBRARY ${LLDB_FRAMEWORK})
            set(HAS_LLDB TRUE)
        elseif(LLDB_INCLUDE_DIR)
            # No system framework — fall back to Homebrew's liblldb for both
            find_library(LLDB_LIBRARY NAMES lldb liblldb
                PATHS /usr/local/opt/llvm/lib /opt/homebrew/opt/llvm/lib)
            if(LLDB_LIBRARY)
                message(STATUS "LLDB (Homebrew): ${LLDB_LIBRARY}")
                message(STATUS "LLDB headers: ${LLDB_INCLUDE_DIR}")
                set(HAS_LLDB TRUE)
            endif()
        endif()
    else()
        # Linux: find liblldb
        find_library(LLDB_LIBRARY NAMES lldb liblldb
            PATHS
                /usr/lib/llvm-18/lib
                /usr/lib/llvm-17/lib
                /usr/lib/llvm-16/lib
                /usr/lib/llvm-15/lib
                /usr/lib/llvm-14/lib
                /usr/local/opt/llvm/lib
                /opt/homebrew/opt/llvm/lib
        )
        if(LLDB_INCLUDE_DIR AND LLDB_LIBRARY)
            message(STATUS "LLDB found: ${LLDB_LIBRARY}")
            message(STATUS "LLDB headers: ${LLDB_INCLUDE_DIR}")
            set(HAS_LLDB TRUE)
        endif()
    endif()

    if(NOT HAS_LLDB)
        message(WARNING "LLDB not found -- building UI-only (no debug backend)")
    endif()
else()
    set(HAS_LLDB FALSE)
endif()

# --- Sources ---
set(SOURCES
    src/main.cpp

    # Common
    src/common/Configuration.h
    src/common/Configuration.cpp

    # Core
    src/core/DebugCore.h
    src/core/DebugCore.cpp

    # GUI
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
    src/gui/CPUWidget.h
    src/gui/CPUWidget.cpp

    # Widgets
    src/gui/widgets/CPUDisassembly.h
    src/gui/widgets/CPUDisassembly.cpp
    src/gui/widgets/CPUInfoBox.h
    src/gui/widgets/CPUInfoBox.cpp
    src/gui/widgets/CPUSideBar.h
    src/gui/widgets/CPUSideBar.cpp
    src/gui/widgets/CPURegistersView.h
    src/gui/widgets/CPURegistersView.cpp
    src/gui/widgets/CPUStack.h
    src/gui/widgets/CPUStack.cpp
    src/gui/widgets/CPUDump.h
    src/gui/widgets/CPUDump.cpp
    src/gui/widgets/CPUArgumentWidget.h
    src/gui/widgets/CPUArgumentWidget.cpp
    src/gui/widgets/BreakpointsView.h
    src/gui/widgets/BreakpointsView.cpp
    src/gui/widgets/MemoryMapView.h
    src/gui/widgets/MemoryMapView.cpp
    src/gui/widgets/CallStackView.h
    src/gui/widgets/CallStackView.cpp
    src/gui/widgets/ThreadsView.h
    src/gui/widgets/ThreadsView.cpp
    src/gui/widgets/LogView.h
    src/gui/widgets/LogView.cpp
    src/gui/widgets/CommandLineEdit.h
    src/gui/widgets/CommandLineEdit.cpp

    # Dialogs
    src/gui/dialogs/GotoDialog.h
    src/gui/dialogs/GotoDialog.cpp
)

# --- Resources ---
set(RESOURCES resources/resources.qrc)

# --- Target ---
add_executable(x64lldbg ${SOURCES} ${RESOURCES})

target_include_directories(x64lldbg PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(x64lldbg PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

if(HAS_LLDB)
    target_compile_definitions(x64lldbg PRIVATE HAS_LLDB)
    target_link_libraries(x64lldbg PRIVATE ${LLDB_LIBRARY})

    target_include_directories(x64lldbg PRIVATE ${LLDB_INCLUDE_DIR})

    # Smoke test (headless, no GUI)
    add_executable(smoke_test
        test/smoke_test.cpp
        src/core/DebugCore.h
        src/core/DebugCore.cpp
        src/common/Configuration.h
        src/common/Configuration.cpp
    )
    target_compile_definitions(smoke_test PRIVATE HAS_LLDB)
    target_link_libraries(smoke_test PRIVATE Qt6::Core Qt6::Gui ${LLDB_LIBRARY})

    target_include_directories(smoke_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${LLDB_INCLUDE_DIR})
endif()
