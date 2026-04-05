#include <QApplication>
#include <QFileInfo>
#include <cstdlib>

#include "common/Configuration.h"
#include "core/DebugCore.h"
#include "gui/MainWindow.h"

static void findLLDBServer()
{
    // If already set, don't override
    if (qEnvironmentVariableIsSet("LLDB_DEBUGSERVER_PATH"))
        return;

    // Common paths for lldb-server on Linux.
    // On macOS, DebugCore sets LLDB_DEBUGSERVER_PATH to Apple's signed
    // debugserver; Homebrew's lldb-server lacks entitlements and will
    // cause a handshake timeout.
    static const char* paths[] = {
#ifndef __APPLE__
        "/usr/lib/llvm-18/bin/lldb-server",
        "/usr/lib/llvm-17/bin/lldb-server",
        "/usr/lib/llvm-16/bin/lldb-server",
        "/usr/lib/llvm-15/bin/lldb-server",
        "/usr/lib/llvm-14/bin/lldb-server",
        "/usr/local/opt/llvm/bin/lldb-server",
        "/opt/homebrew/opt/llvm/bin/lldb-server",
        "/usr/bin/lldb-server",
#endif
        nullptr
    };

    for (const char** p = paths; *p; ++p) {
        if (QFileInfo::exists(*p)) {
            qputenv("LLDB_DEBUGSERVER_PATH", *p);
            return;
        }
    }
}

int main(int argc, char* argv[])
{
    findLLDBServer();

    QApplication app(argc, argv);
    app.setApplicationName("x64lldbg");
    app.setApplicationVersion("0.1.0");

    // Initialize configuration singleton
    Config();

    // Create debug core
    DebugCore debugCore;

    // Create and show main window
    MainWindow mainWindow(&debugCore);
    mainWindow.show();

    return app.exec();
}
