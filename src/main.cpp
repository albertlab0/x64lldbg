#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
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

    // Optional positional arg: path to executable to debug, followed by
    // args forwarded to the debuggee.
    //   x64lldbg [<program> [<arg>...]]
    QCommandLineParser parser;
    parser.addPositionalArgument("program",
        "Executable to debug (supports ~ and $VAR expansion)");
    parser.addPositionalArgument("args", "Arguments to pass to the program",
        "[args...]");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        QString programPath = expandPath(positional.first());
        QStringList programArgs = positional.mid(1);

        QTimer::singleShot(0, &debugCore, [&debugCore, programPath, programArgs]() {
            QStringList archs = DebugCore::detectArchitectures(programPath);
            QString arch = (archs.size() == 1) ? archs.first() : QString();
            debugCore.startDebug(programPath, programArgs, arch);
        });
    }

    return app.exec();
}
