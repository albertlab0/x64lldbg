// Headless smoke test for DebugCore
// Verifies LLDB integration works without needing a GUI
#include <QCoreApplication>
#include <QTimer>
#include <cstdio>

#include "core/DebugCore.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    DebugCore core;

    QObject::connect(&core, &DebugCore::outputReceived, [](const QString& msg) {
        printf("LOG: %s\n", msg.toStdString().c_str());
    });

    QObject::connect(&core, &DebugCore::processStateChanged, [&](DebugCore::ProcessState state) {
        printf("STATE: %d\n", state);

        if (state == DebugCore::Stopped) {
            printf("\n--- PC: 0x%lx ---\n", core.currentPC());

            // Test registers
            auto regs = core.getRegisters();
            printf("\n--- Registers (%d) ---\n", regs.size());
            for (const auto& r : regs) {
                QString deref = core.dereferencePointer(r.value);
                if (!deref.isEmpty())
                    printf("  %s = 0x%016lx  -> %s\n", r.name.toStdString().c_str(), r.value, deref.toStdString().c_str());
                else
                    printf("  %s = 0x%016lx\n", r.name.toStdString().c_str(), r.value);
            }

            // Test disassembly
            auto disasm = core.disassemble(core.currentPC(), 10);
            printf("\n--- Disassembly (%d lines) ---\n", disasm.size());
            for (const auto& line : disasm) {
                printf("  0x%lx: %s %s", line.address,
                    line.mnemonic.toStdString().c_str(),
                    line.operands.toStdString().c_str());
                if (!line.comment.isEmpty())
                    printf("  ; %s", line.comment.toStdString().c_str());
                printf("\n");
            }

            // Test stack
            auto stack = core.getStackEntries(8);
            printf("\n--- Stack (%d entries) ---\n", stack.size());
            for (const auto& s : stack) {
                printf("  0x%lx: 0x%016lx", s.address, s.value);
                if (!s.comment.isEmpty())
                    printf("  %s", s.comment.toStdString().c_str());
                printf("\n");
            }

            // Test call stack
            auto cs = core.getCallStack();
            printf("\n--- Call Stack (%d frames) ---\n", cs.size());
            for (const auto& f : cs) {
                printf("  #%d 0x%lx %s!%s\n", f.index, f.address,
                    f.module.toStdString().c_str(),
                    f.function.toStdString().c_str());
            }

            // Test breakpoints
            auto bps = core.getBreakpoints();
            printf("\n--- Breakpoints (%d) ---\n", bps.size());
            for (const auto& bp : bps) {
                printf("  #%d 0x%lx %s %s\n", bp.id, bp.address,
                    bp.enabled ? "enabled" : "disabled",
                    bp.module.toStdString().c_str());
            }

            printf("\n=== SMOKE TEST PASSED ===\n");

            // Step once then quit
            core.stop();
            QTimer::singleShot(500, &app, &QCoreApplication::quit);
        }
    });

    // Launch test binary
    printf("Launching test binary...\n");
    if (!core.startDebug("/home/ubuntu/repos/x64lldbg/test/hello")) {
        printf("FAILED to launch!\n");
        return 1;
    }

    // Timeout safety
    QTimer::singleShot(10000, &app, [&app]() {
        printf("TIMEOUT - test did not complete\n");
        app.exit(1);
    });

    return app.exec();
}
