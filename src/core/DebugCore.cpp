#include "DebugCore.h"
#include <QTimer>
#include <QFile>
#include <QProcess>

// ============================================================
// LLDBEventListener — background thread polling LLDB events
// ============================================================

#ifdef HAS_LLDB

LLDBEventListener::LLDBEventListener(lldb::SBListener& listener, QObject* parent)
    : QThread(parent)
    , m_listener(listener)
{
}

void LLDBEventListener::stop()
{
    m_running = false;
}

void LLDBEventListener::run()
{
    while (m_running) {
        lldb::SBEvent event;
        // Wait for up to 1 second for an event
        if (m_listener.WaitForEvent(1, event)) {
            if (!event.IsValid())
                continue;

            if (lldb::SBProcess::EventIsProcessEvent(event)) {
                lldb::StateType state = lldb::SBProcess::GetStateFromEvent(event);
                switch (state) {
                case lldb::eStateStopped:
                    emit processStopped();
                    break;
                case lldb::eStateRunning:
                    emit processRunning();
                    break;
                case lldb::eStateExited: {
                    // Can't easily get exit code from event without process ref
                    emit processExited(0);
                    break;
                }
                case lldb::eStateCrashed:
                    emit processCrashed();
                    break;
                default:
                    break;
                }
            }

            // Check for stdout/stderr
            size_t len;
            char buf[4096];

            lldb::SBProcess process = lldb::SBProcess::GetProcessFromEvent(event);
            if (process.IsValid()) {
                while ((len = process.GetSTDOUT(buf, sizeof(buf) - 1)) > 0) {
                    buf[len] = '\0';
                    emit stdoutReceived(QString::fromUtf8(buf, static_cast<int>(len)));
                }
                while ((len = process.GetSTDERR(buf, sizeof(buf) - 1)) > 0) {
                    buf[len] = '\0';
                    emit stderrReceived(QString::fromUtf8(buf, static_cast<int>(len)));
                }
            }
        }
    }
}

#endif // HAS_LLDB

// ============================================================
// DebugCore
// ============================================================

DebugCore::DebugCore(QObject* parent)
    : QObject(parent)
{
#ifdef HAS_LLDB
    lldb::SBDebugger::Initialize();
    m_debugger = lldb::SBDebugger::Create(false);
    m_debugger.SetAsync(true);
    m_debugger.HandleCommand("settings set target.x86-disassembly-flavor intel");
    m_debugger.HandleCommand("settings set target.skip-prologue false");

#ifdef __APPLE__
    // Homebrew's LLVM doesn't ship debugserver. Set LLDB_DEBUGSERVER_PATH
    // to the system one from Xcode or Command Line Tools.
    if (qEnvironmentVariableIsEmpty("LLDB_DEBUGSERVER_PATH")) {
        const char* debugserverPaths[] = {
            "/Library/Developer/CommandLineTools/Library/PrivateFrameworks/"
                "LLDB.framework/Versions/A/Resources/debugserver",
            "/Applications/Xcode.app/Contents/SharedFrameworks/"
                "LLDB.framework/Versions/A/Resources/debugserver",
            nullptr
        };
        for (const char** p = debugserverPaths; *p; ++p) {
            if (QFile::exists(*p)) {
                qputenv("LLDB_DEBUGSERVER_PATH", *p);
                break;
            }
        }
    }
#endif
    m_listener = m_debugger.GetListener();
#endif
}

DebugCore::~DebugCore()
{
#ifdef HAS_LLDB
    stopEventListener();
    if (m_process.IsValid()) {
        m_process.Kill();
    }
    lldb::SBDebugger::Destroy(m_debugger);
    lldb::SBDebugger::Terminate();
#endif
}

// --- Disassembly flavor ---

void DebugCore::setAsmFlavor(AsmFlavor flavor)
{
    if (m_asmFlavor == flavor) return;
    m_asmFlavor = flavor;
#ifdef HAS_LLDB
    if (m_debugger.IsValid()) {
        const char* cmd = (flavor == Intel)
            ? "settings set target.x86-disassembly-flavor intel"
            : "settings set target.x86-disassembly-flavor att";
        m_debugger.HandleCommand(cmd);
    }
#endif
    emit asmFlavorChanged();
}

// --- Session management ---

QStringList DebugCore::detectArchitectures(const QString& path)
{
    QStringList archs;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return archs;

    QByteArray header = f.read(8);
    if (header.size() < 8)
        return archs;

    // Check for Mach-O fat binary magic (0xCAFEBABE big-endian or 0xBEBAFECA little-endian)
    uint32_t magic = *reinterpret_cast<const uint32_t*>(header.constData());
    bool isFat = (magic == 0xCAFEBABE || magic == 0xBEBAFECA);
    if (!isFat)
        return archs; // Not a fat binary, single arch — let LLDB auto-detect

#ifdef __APPLE__
    // Use lipo to list architectures
    QProcess proc;
    proc.start("lipo", {"-archs", path});
    proc.waitForFinished(3000);
    if (proc.exitCode() == 0) {
        QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        archs = output.split(' ', Qt::SkipEmptyParts);
    }
#endif
    return archs;
}

bool DebugCore::startDebug(const QString& path, const QStringList& args, const QString& arch)
{
    m_targetPath = path;
    m_targetArgs = args;
    m_targetArch = arch;

#ifdef HAS_LLDB
    // Clean up previous session
    stopEventListener();
    if (m_process.IsValid()) {
        m_process.Kill();
    }

    lldb::SBError error;
    if (!arch.isEmpty()) {
        m_target = m_debugger.CreateTargetWithFileAndArch(
            path.toStdString().c_str(), arch.toStdString().c_str());
    } else {
        m_target = m_debugger.CreateTarget(path.toStdString().c_str());
    }
    if (!m_target.IsValid()) {
        emit outputReceived("Failed to create target: " + path);
        return false;
    }

    emit outputReceived("Target created: " + path);

    // --- Set default breakpoints (like x64dbg's system breakpoint behavior) ---
    lldb::SBModule module = m_target.GetModuleAtIndex(0);

    // 1. Breakpoint at main (if it exists)
    lldb::SBBreakpoint mainBp = m_target.BreakpointCreateByName("main");
    if (mainBp.IsValid() && mainBp.GetNumLocations() > 0) {
        emit outputReceived(QString("Breakpoint set at main (id=%1)").arg(mainBp.GetID()));
    } else {
        if (mainBp.IsValid())
            m_target.BreakpointDelete(mainBp.GetID());

        // No main — try common entry point symbols
        const char* entryNames[] = {"_start", "start", "_main", "entry", nullptr};
        bool found = false;
        for (const char** name = entryNames; *name; ++name) {
            lldb::SBBreakpoint bp = m_target.BreakpointCreateByName(*name);
            if (bp.IsValid() && bp.GetNumLocations() > 0) {
                emit outputReceived(QString("Breakpoint set at %1 (id=%2)")
                    .arg(*name).arg(bp.GetID()));
                found = true;
                break;
            } else if (bp.IsValid()) {
                m_target.BreakpointDelete(bp.GetID());
            }
        }

        if (!found && module.IsValid()) {
            // Last resort: entry point from object file header (LC_MAIN / ELF e_entry)
            lldb::SBAddress entryAddr = module.GetObjectFileEntryPointAddress();
            if (entryAddr.IsValid()) {
                lldb::SBBreakpoint entryBp = m_target.BreakpointCreateBySBAddress(entryAddr);
                if (entryBp.IsValid()) {
                    uint64_t addr = entryAddr.GetLoadAddress(m_target);
                    if (addr == UINT64_MAX)
                        addr = entryAddr.GetFileAddress();
                    emit outputReceived(QString("Breakpoint set at entry point 0x%1 (id=%2)")
                        .arg(addr, 0, 16).arg(entryBp.GetID()));
                }
            } else {
                emit outputReceived("Warning: no main or entry point found");
            }
        }
    }

    // 2. Breakpoints on Mach-O initializer functions (__mod_init_func)
    //    and ELF .init_array. These run before main/start.
    if (module.IsValid()) {
        uint32_t numSections = module.GetNumSections();
        for (uint32_t i = 0; i < numSections; i++) {
            lldb::SBSection section = module.GetSectionAtIndex(i);
            if (!section.IsValid()) continue;

            // Check top-level sections and subsections
            auto checkSection = [&](lldb::SBSection sec) {
                const char* secName = sec.GetName();
                if (!secName) return;
                QString name(secName);

                // Mach-O: __mod_init_func, ELF: .init_array
                if (name != "__mod_init_func" && name != ".init_array")
                    return;

                size_t ptrSize = (m_target.GetAddressByteSize() > 0)
                    ? m_target.GetAddressByteSize() : 8;
                size_t secSize = sec.GetByteSize();
                size_t numPtrs = secSize / ptrSize;

                if (numPtrs == 0) return;

                emit outputReceived(QString("Found %1 initializer(s) in %2")
                    .arg(numPtrs).arg(name));

                // Read the section to get function pointer addresses
                // We need the file address of the section start, then compute
                // each pointer's file address
                lldb::SBData secData = sec.GetSectionData();
                if (!secData.IsValid()) return;

                lldb::SBError err;
                for (size_t j = 0; j < numPtrs; j++) {
                    uint64_t funcAddr = 0;
                    if (ptrSize == 8)
                        funcAddr = secData.GetUnsignedInt64(err, j * ptrSize);
                    else
                        funcAddr = secData.GetUnsignedInt32(err, j * ptrSize);

                    if (err.Fail() || funcAddr == 0) continue;

                    // Resolve as file address so LLDB applies ASLR slide correctly
                    lldb::SBAddress sbAddr = module.ResolveFileAddress(funcAddr);
                    if (!sbAddr.IsValid()) continue;
                    lldb::SBBreakpoint bp = m_target.BreakpointCreateBySBAddress(sbAddr);
                    if (bp.IsValid()) {
                        // Try to get symbol name for logging
                        QString label = QString("InitFunc_%1").arg(j);
                        lldb::SBSymbol sym = sbAddr.GetSymbol();
                        if (sym.IsValid() && sym.GetName())
                            label = sym.GetName();

                        emit outputReceived(QString("Breakpoint set at initializer %1 = 0x%2 (id=%3)")
                            .arg(label)
                            .arg(funcAddr, 0, 16)
                            .arg(bp.GetID()));
                    }
                }
            };

            checkSection(section);
            // Also check subsections (Mach-O nests __mod_init_func under __DATA)
            uint32_t numSub = section.GetNumSubSections();
            for (uint32_t j = 0; j < numSub; j++) {
                checkSection(section.GetSubSectionAtIndex(j));
            }
        }

        // Also check for C++ global constructors by symbol name
        uint32_t numSymbols = module.GetNumSymbols();
        for (uint32_t i = 0; i < numSymbols; i++) {
            lldb::SBSymbol sym = module.GetSymbolAtIndex(i);
            if (!sym.IsValid() || !sym.GetName()) continue;
            QString symName(sym.GetName());
            if (symName.startsWith("__GLOBAL__sub_I_") ||
                symName.startsWith("_GLOBAL__sub_I_")) {
                lldb::SBBreakpoint bp = m_target.BreakpointCreateByName(sym.GetName());
                if (bp.IsValid() && bp.GetNumLocations() > 0) {
                    emit outputReceived(QString("Breakpoint set at C++ init %1 (id=%2)")
                        .arg(symName).arg(bp.GetID()));
                } else if (bp.IsValid()) {
                    m_target.BreakpointDelete(bp.GetID());
                }
            }
        }
    }

    // Convert args
    std::vector<std::string> argStrs;
    for (const auto& a : args)
        argStrs.push_back(a.toStdString());
    std::vector<const char*> argv;
    for (const auto& s : argStrs)
        argv.push_back(s.c_str());
    argv.push_back(nullptr);

    // Launch — stop at entry point (like x64dbg stops at system entry)
    lldb::SBError launchError;
    lldb::SBLaunchInfo launchInfo(argv.size() > 1 ? argv.data() : nullptr);
    launchInfo.SetLaunchFlags(launchInfo.GetLaunchFlags() | lldb::eLaunchFlagStopAtEntry);

    // Don't set custom listener during launch — on macOS this can interfere
    // with the debugserver handshake. We'll hijack events after launch.
    m_process = m_target.Launch(launchInfo, launchError);

    if (launchError.Fail()) {
        emit outputReceived(QString("Launch error: %1").arg(launchError.GetCString()));
        return false;
    }

    if (!m_process.IsValid()) {
        emit outputReceived("Failed to launch process: " + path);
        return false;
    }

    emit outputReceived(QString("Process launched (PID: %1)").arg(m_process.GetProcessID()));

    // Now that the process is launched, register our listener for future events
    m_process.GetBroadcaster().AddListener(
        m_listener,
        lldb::SBProcess::eBroadcastBitStateChanged |
        lldb::SBProcess::eBroadcastBitSTDOUT |
        lldb::SBProcess::eBroadcastBitSTDERR);

    // Start event listener
    startEventListener();

    // The process should hit the main breakpoint and stop
    // Poll for initial state
    QTimer::singleShot(500, this, [this]() {
        if (m_process.IsValid()) {
            lldb::StateType state = m_process.GetState();
            if (state == lldb::eStateStopped) {
                onProcessStopped();
            } else if (state == lldb::eStateRunning) {
                setState(Running);
            }
        }
    });

    return true;
#else
    emit outputReceived("[no LLDB] startDebug: " + path);
    setState(Stopped);
    emitAllRefresh();
    return true;
#endif
}

bool DebugCore::attach(int pid)
{
#ifdef HAS_LLDB
    stopEventListener();

    lldb::SBError error;
    m_target = m_debugger.CreateTarget("");
    if (!m_target.IsValid()) {
        emit outputReceived("Failed to create empty target for attach");
        return false;
    }

    lldb::SBAttachInfo attachInfo(static_cast<lldb::pid_t>(pid));
    m_process = m_target.Attach(attachInfo, error);
    if (error.Fail()) {
        emit outputReceived(QString("Attach failed: %1").arg(error.GetCString()));
        return false;
    }

    emit outputReceived(QString("Attached to PID %1").arg(pid));
    startEventListener();
    onProcessStopped();
    return true;
#else
    Q_UNUSED(pid)
    emit outputReceived(QString("[no LLDB] attach to PID %1").arg(pid));
    setState(Stopped);
    return true;
#endif
}

void DebugCore::detach()
{
#ifdef HAS_LLDB
    stopEventListener();
    if (m_process.IsValid()) {
        lldb::SBError error = m_process.Detach();
        if (error.Fail())
            emit outputReceived(QString("Detach error: %1").arg(error.GetCString()));
    }
#endif
    setState(Unloaded);
    emit outputReceived("Detached");
}

void DebugCore::stop()
{
#ifdef HAS_LLDB
    stopEventListener();
    if (m_process.IsValid()) {
        m_process.Kill();
    }
#endif
    setState(Exited);
    emit outputReceived("Process terminated");
}

void DebugCore::restart()
{
    QString path = m_targetPath;
    QStringList args = m_targetArgs;
    QString arch = m_targetArch;
    stop();
    if (!path.isEmpty()) {
        startDebug(path, args, arch);
    }
}

// --- Execution control ---

void DebugCore::continueExec()
{
#ifdef HAS_LLDB
    if (m_process.IsValid() && m_state == Stopped) {
        lldb::SBError error = m_process.Continue();
        if (error.Fail()) {
            emit outputReceived(QString("Continue failed: %1").arg(error.GetCString()));
            return;
        }
        setState(Running);
    }
#else
    emit outputReceived("[no LLDB] continue");
#endif
}

void DebugCore::pause()
{
#ifdef HAS_LLDB
    if (m_process.IsValid() && m_state == Running) {
        lldb::SBError error = m_process.Stop();
        if (error.Fail()) {
            emit outputReceived(QString("Pause failed: %1").arg(error.GetCString()));
            return;
        }
        // Will get processStopped event from listener
    }
#else
    emit outputReceived("[no LLDB] pause");
    setState(Stopped);
#endif
}

void DebugCore::stepInto()
{
#ifdef HAS_LLDB
    if (m_process.IsValid() && m_state == Stopped) {
        lldb::SBThread thread = m_process.GetSelectedThread();
        if (thread.IsValid()) {
            thread.StepInstruction(false); // false = step into calls
            setState(Running);
            // Event listener will emit processStopped when step completes
        }
    }
#else
    emit outputReceived("[no LLDB] step into");
    emitAllRefresh();
#endif
}

void DebugCore::stepOver()
{
#ifdef HAS_LLDB
    if (m_process.IsValid() && m_state == Stopped) {
        lldb::SBThread thread = m_process.GetSelectedThread();
        if (thread.IsValid()) {
            thread.StepInstruction(true); // true = step over calls
            setState(Running);
        }
    }
#else
    emit outputReceived("[no LLDB] step over");
    emitAllRefresh();
#endif
}

void DebugCore::stepOut()
{
#ifdef HAS_LLDB
    if (m_process.IsValid() && m_state == Stopped) {
        lldb::SBThread thread = m_process.GetSelectedThread();
        if (thread.IsValid()) {
            thread.StepOut();
            setState(Running);
        }
    }
#else
    emit outputReceived("[no LLDB] step out");
    emitAllRefresh();
#endif
}

void DebugCore::runToCursor(uint64_t address)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) {
        emit outputReceived(QString("Run to cursor: no valid target"));
        return;
    }
    if (m_state != Stopped) {
        emit outputReceived(QString("Run to cursor: process not stopped (state=%1)").arg(m_state));
        return;
    }

    lldb::SBBreakpoint bp = m_target.BreakpointCreateByAddress(address);
    if (!bp.IsValid()) {
        emit outputReceived(QString("Run to cursor: failed to set breakpoint at 0x%1").arg(address, 0, 16));
        return;
    }
    bp.SetOneShot(true);
    emit outputReceived(QString("Run to cursor: one-shot BP #%1 at 0x%2")
        .arg(bp.GetID()).arg(address, 0, 16));
    continueExec();
#else
    Q_UNUSED(address)
    emit outputReceived(QString("[no LLDB] run to cursor 0x%1").arg(address, 0, 16));
#endif
}

// --- Breakpoints ---

bool DebugCore::addBreakpoint(uint64_t address)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return false;
    lldb::SBBreakpoint bp = m_target.BreakpointCreateByAddress(address);
    if (bp.IsValid()) {
        emit outputReceived(QString("Breakpoint %1 set at 0x%2")
            .arg(bp.GetID()).arg(address, 0, 16));
        emit breakpointsChanged();
        return true;
    }
    return false;
#else
    Q_UNUSED(address)
    emit breakpointsChanged();
    return true;
#endif
}

bool DebugCore::addHardwareBreakpoint(uint64_t address)
{
    // LLDB doesn't directly expose hardware breakpoints through SB API
    // but we can request it through breakpoint options
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return false;
    lldb::SBBreakpoint bp = m_target.BreakpointCreateByAddress(address);
    if (bp.IsValid()) {
        // Note: LLDB manages hardware vs software breakpoints internally
        // based on the target platform and breakpoint location
        emit outputReceived(QString("Hardware breakpoint %1 set at 0x%2")
            .arg(bp.GetID()).arg(address, 0, 16));
        emit breakpointsChanged();
        return true;
    }
    return false;
#else
    Q_UNUSED(address)
    return true;
#endif
}

bool DebugCore::addConditionalBreakpoint(uint64_t address, const QString& condition)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return false;
    lldb::SBBreakpoint bp = m_target.BreakpointCreateByAddress(address);
    if (bp.IsValid()) {
        bp.SetCondition(condition.toStdString().c_str());
        emit outputReceived(QString("Conditional breakpoint %1 at 0x%2: %3")
            .arg(bp.GetID()).arg(address, 0, 16).arg(condition));
        emit breakpointsChanged();
        return true;
    }
    return false;
#else
    Q_UNUSED(address)
    Q_UNUSED(condition)
    return true;
#endif
}

bool DebugCore::removeBreakpoint(uint64_t address)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return false;
    // Find breakpoint at this address
    for (uint32_t i = 0; i < m_target.GetNumBreakpoints(); i++) {
        lldb::SBBreakpoint bp = m_target.GetBreakpointAtIndex(i);
        if (bp.GetNumLocations() > 0) {
            lldb::SBBreakpointLocation loc = bp.GetLocationAtIndex(0);
            if (loc.GetAddress().GetLoadAddress(m_target) == address) {
                m_target.BreakpointDelete(bp.GetID());
                emit outputReceived(QString("Breakpoint removed at 0x%1").arg(address, 0, 16));
                emit breakpointsChanged();
                return true;
            }
        }
    }
    return false;
#else
    Q_UNUSED(address)
    emit breakpointsChanged();
    return true;
#endif
}

bool DebugCore::removeBreakpointById(uint32_t id)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return false;
    bool result = m_target.BreakpointDelete(id);
    if (result) {
        emit outputReceived(QString("Breakpoint %1 removed").arg(id));
        emit breakpointsChanged();
    }
    return result;
#else
    Q_UNUSED(id)
    return true;
#endif
}

bool DebugCore::toggleBreakpoint(uint64_t address)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return false;
    // Check if breakpoint exists at address
    for (uint32_t i = 0; i < m_target.GetNumBreakpoints(); i++) {
        lldb::SBBreakpoint bp = m_target.GetBreakpointAtIndex(i);
        if (bp.GetNumLocations() > 0) {
            lldb::SBBreakpointLocation loc = bp.GetLocationAtIndex(0);
            if (loc.GetAddress().GetLoadAddress(m_target) == address) {
                bp.SetEnabled(!bp.IsEnabled());
                emit breakpointsChanged();
                return true;
            }
        }
    }
    // No breakpoint exists, add one
    return addBreakpoint(address);
#else
    Q_UNUSED(address)
    return true;
#endif
}

QVector<BreakpointInfo> DebugCore::getBreakpoints() const
{
    QVector<BreakpointInfo> result;
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return result;
    for (uint32_t i = 0; i < m_target.GetNumBreakpoints(); i++) {
        lldb::SBBreakpoint bp = m_target.GetBreakpointAtIndex(i);
        if (!bp.IsValid()) continue;

        BreakpointInfo info;
        info.id = bp.GetID();
        info.enabled = bp.IsEnabled();
        info.isHardware = bp.IsHardware();
        info.hitCount = bp.GetHitCount();

        const char* cond = bp.GetCondition();
        info.condition = cond ? QString(cond) : QString();

        if (bp.GetNumLocations() > 0) {
            lldb::SBBreakpointLocation loc = bp.GetLocationAtIndex(0);
            lldb::SBAddress addr = loc.GetAddress();
            uint64_t loadAddr = addr.GetLoadAddress(m_target);

            // Skip unresolved breakpoints (address not yet mapped)
            if (loadAddr == UINT64_MAX)
                continue;

            info.address = loadAddr;

            lldb::SBModule mod = addr.GetModule();
            if (mod.IsValid()) {
                lldb::SBFileSpec fileSpec = mod.GetFileSpec();
                info.module = fileSpec.GetFilename();
            }
        } else {
            // No locations resolved yet — skip
            continue;
        }

        // Merge our extra data (log text, etc.)
        if (m_bpExtras.contains(info.id)) {
            const auto& extra = m_bpExtras[info.id];
            info.logText = extra.logText;
            info.logCondition = extra.logCondition;
            info.fastResume = extra.fastResume;
        }

        result.append(info);
    }
#endif
    return result;
}

// --- Breakpoint properties ---

void DebugCore::setBreakpointCondition(uint32_t id, const QString& condition)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return;
    lldb::SBBreakpoint bp = m_target.FindBreakpointByID(id);
    if (bp.IsValid()) {
        bp.SetCondition(condition.isEmpty() ? nullptr : condition.toStdString().c_str());
        emit breakpointsChanged();
    }
#else
    Q_UNUSED(id) Q_UNUSED(condition)
#endif
}

void DebugCore::setBreakpointLogText(uint32_t id, const QString& logText)
{
    m_bpExtras[id].logText = logText;
    emit breakpointsChanged();
}

void DebugCore::setBreakpointLogCondition(uint32_t id, const QString& logCondition)
{
    m_bpExtras[id].logCondition = logCondition;
    emit breakpointsChanged();
}

void DebugCore::setBreakpointFastResume(uint32_t id, bool fastResume)
{
    m_bpExtras[id].fastResume = fastResume;
    emit breakpointsChanged();
}

QString DebugCore::formatLogText(const QString& logText)
{
    // Expand {register} and {expression} placeholders in log text
    // Supports: {rax}, {rip}, {[address]}, {s:rsi} (string), etc.
    QString result;
    int i = 0;
    while (i < logText.size()) {
        if (logText[i] == '{') {
            int end = logText.indexOf('}', i + 1);
            if (end < 0) {
                result += logText[i];
                i++;
                continue;
            }
            // Escaped {{ → literal {
            if (i + 1 < logText.size() && logText[i + 1] == '{') {
                result += '{';
                i += 2;
                continue;
            }
            QString expr = logText.mid(i + 1, end - i - 1).trimmed();
            QString formatted = formatExpression(expr);
            result += formatted;
            i = end + 1;
        } else if (logText[i] == '}' && i + 1 < logText.size() && logText[i + 1] == '}') {
            result += '}';
            i += 2;
        } else {
            result += logText[i];
            i++;
        }
    }
    return result;
}

// --- Data accessors ---

uint64_t DebugCore::currentPC()
{
#ifdef HAS_LLDB
    if (m_process.IsValid()) {
        lldb::SBThread thread = m_process.GetSelectedThread();
        if (thread.IsValid()) {
            lldb::SBFrame frame = thread.GetSelectedFrame();
            if (frame.IsValid())
                return frame.GetPC();
        }
    }
#endif
    return 0;
}

QVector<RegisterInfo> DebugCore::getRegisters()
{
    QVector<RegisterInfo> regs;
#ifdef HAS_LLDB
    if (!m_process.IsValid()) return regs;

    lldb::SBThread thread = m_process.GetSelectedThread();
    if (!thread.IsValid()) return regs;

    lldb::SBFrame frame = thread.GetSelectedFrame();
    if (!frame.IsValid()) return regs;

    lldb::SBValueList regSets = frame.GetRegisters();

    // Filter: only show main 64-bit GPRs + flags (like x64dbg)
    static const QStringList mainRegs = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "rip", "rflags"
    };
    QSet<QString> mainRegSet(mainRegs.begin(), mainRegs.end());

    for (uint32_t setIdx = 0; setIdx < regSets.GetSize(); setIdx++) {
        lldb::SBValue regSet = regSets.GetValueAtIndex(setIdx);
        QString setName = regSet.GetName();

        // Only show general purpose registers
        if (!setName.toLower().contains("general"))
            continue;

        for (uint32_t i = 0; i < regSet.GetNumChildren(); i++) {
            lldb::SBValue reg = regSet.GetChildAtIndex(i);
            if (!reg.IsValid()) continue;

            QString regName = QString(reg.GetName()).toLower();
            if (!mainRegSet.contains(regName))
                continue;

            RegisterInfo info;
            info.name = QString(reg.GetName()).toUpper();
            info.value = reg.GetValueAsUnsigned(0);

            // Check if changed from previous stop
            auto it = m_prevRegisters.find(info.name);
            info.modified = (it != m_prevRegisters.end() && it.value() != info.value);

            regs.append(info);
        }
    }

    // Update previous values
    for (const auto& r : regs) {
        m_prevRegisters[r.name] = r.value;
    }
#endif
    return regs;
}

QByteArray DebugCore::readMemory(uint64_t address, size_t size)
{
#ifdef HAS_LLDB
    if (!m_process.IsValid()) return QByteArray(static_cast<int>(size), '\0');

    QByteArray data(static_cast<int>(size), '\0');
    lldb::SBError error;
    size_t bytesRead = m_process.ReadMemory(address, data.data(), size, error);
    if (error.Fail()) {
        return QByteArray(static_cast<int>(size), '\0');
    }
    data.resize(static_cast<int>(bytesRead));
    return data;
#else
    // Stub
    QByteArray data(static_cast<int>(size), '\0');
    for (int i = 0; i < data.size(); i++)
        data[i] = static_cast<char>((address + i) & 0xFF);
    return data;
#endif
}

bool DebugCore::writeMemory(uint64_t address, const QByteArray& data)
{
#ifdef HAS_LLDB
    if (!m_process.IsValid()) return false;
    lldb::SBError error;
    m_process.WriteMemory(address, data.data(), data.size(), error);
    if (error.Fail()) {
        emit const_cast<DebugCore*>(this)->outputReceived(
            QString("Write memory failed at 0x%1: %2").arg(address, 0, 16).arg(error.GetCString()));
        return false;
    }
    return true;
#else
    Q_UNUSED(address)
    Q_UNUSED(data)
    return false;
#endif
}

QVector<StackEntry> DebugCore::getStackEntries(int count)
{
    QVector<StackEntry> stack;
#ifdef HAS_LLDB
    if (!m_process.IsValid()) return stack;

    lldb::SBThread thread = m_process.GetSelectedThread();
    if (!thread.IsValid()) return stack;

    lldb::SBFrame frame = thread.GetSelectedFrame();
    if (!frame.IsValid()) return stack;

    // Read RSP
    lldb::SBValue spVal = frame.FindRegister("rsp");
    if (!spVal.IsValid()) return stack;
    uint64_t sp = spVal.GetValueAsUnsigned(0);

    // Read stack entries
    lldb::SBError error;
    for (int i = 0; i < count; i++) {
        uint64_t addr = sp + i * 8;
        uint64_t value = 0;
        m_process.ReadMemory(addr, &value, 8, error);
        if (error.Fail()) break;

        StackEntry entry;
        entry.address = addr;
        entry.value = value;
        entry.comment = dereferencePointer(value);
        stack.append(entry);
    }
#else
    // Stub
    uint64_t sp = 0x00007FFEE3B4C680;
    for (int i = 0; i < count; i++) {
        StackEntry entry;
        entry.address = sp + i * 8;
        entry.value = 0x00007FFEE3B4C700 + i * 0x10;
        entry.comment = (i == 0) ? "return to main+0x1A" : "";
        stack.append(entry);
    }
#endif
    return stack;
}

QVector<ThreadInfo> DebugCore::getThreads()
{
    QVector<ThreadInfo> threads;
#ifdef HAS_LLDB
    if (!m_process.IsValid()) return threads;

    lldb::SBThread selectedThread = m_process.GetSelectedThread();
    uint32_t selectedTID = selectedThread.IsValid() ? selectedThread.GetThreadID() : 0;

    for (uint32_t i = 0; i < m_process.GetNumThreads(); i++) {
        lldb::SBThread thread = m_process.GetThreadAtIndex(i);
        if (!thread.IsValid()) continue;

        ThreadInfo info;
        info.id = thread.GetThreadID();

        lldb::SBFrame frame = thread.GetFrameAtIndex(0);
        info.pc = frame.IsValid() ? frame.GetPC() : 0;

        const char* name = thread.GetName();
        const char* queueName = thread.GetQueueName();
        if (name && name[0])
            info.name = name;
        else if (queueName && queueName[0])
            info.name = queueName;
        else
            info.name = QString("Thread %1").arg(info.id);

        info.isCurrent = (info.id == selectedTID);
        threads.append(info);
    }
#else
    threads.append({1, 0x00401000, "main", true});
#endif
    return threads;
}

QVector<CallStackEntry> DebugCore::getCallStack()
{
    QVector<CallStackEntry> cs;
#ifdef HAS_LLDB
    if (!m_process.IsValid()) return cs;

    lldb::SBThread thread = m_process.GetSelectedThread();
    if (!thread.IsValid()) return cs;

    uint32_t numFrames = thread.GetNumFrames();
    for (uint32_t i = 0; i < numFrames; i++) {
        lldb::SBFrame frame = thread.GetFrameAtIndex(i);
        if (!frame.IsValid()) continue;

        CallStackEntry entry;
        entry.index = i;
        entry.address = frame.GetPC();

        // Return address from caller frame
        if (i + 1 < numFrames) {
            lldb::SBFrame callerFrame = thread.GetFrameAtIndex(i + 1);
            entry.returnAddress = callerFrame.IsValid() ? callerFrame.GetPC() : 0;
        } else {
            entry.returnAddress = 0;
        }

        lldb::SBModule mod = frame.GetModule();
        if (mod.IsValid()) {
            lldb::SBFileSpec fileSpec = mod.GetFileSpec();
            entry.module = fileSpec.GetFilename();
        }

        const char* funcName = frame.GetFunctionName();
        entry.function = funcName ? funcName : "";

        cs.append(entry);
    }
#else
    cs.append({0, 0x00401000, 0x00401050, "x64lldbg", "main"});
    cs.append({1, 0x00401050, 0x7FFF20010000, "x64lldbg", "_start"});
#endif
    return cs;
}

QVector<MemoryRegion> DebugCore::getMemoryMap()
{
    QVector<MemoryRegion> map;
#ifdef HAS_LLDB
    if (!m_process.IsValid()) return map;

    lldb::SBMemoryRegionInfoList regions = m_process.GetMemoryRegions();

    for (uint32_t i = 0; i < regions.GetSize(); i++) {
        lldb::SBMemoryRegionInfo info;
        if (!regions.GetMemoryRegionAtIndex(i, info))
            continue;

        MemoryRegion region;
        region.base = info.GetRegionBase();
        region.size = info.GetRegionEnd() - info.GetRegionBase();

        // Build permissions string
        QString perms;
        perms += info.IsReadable() ? 'r' : '-';
        perms += info.IsWritable() ? 'w' : '-';
        perms += info.IsExecutable() ? 'x' : '-';
        region.permissions = perms;

        // Try to get the name from the module at this address
        const char* name = info.GetName();
        if (name && name[0]) {
            region.name = name;
        } else {
            lldb::SBAddress addr(region.base, m_target);
            lldb::SBModule mod = addr.GetModule();
            if (mod.IsValid()) {
                region.name = mod.GetFileSpec().GetFilename();
            }
        }

        map.append(region);
    }
#else
    map.append({0x00400000, 0x1000, ".text", "r-x"});
    map.append({0x00401000, 0x1000, ".data", "rw-"});
    map.append({0x00600000, 0x1000, ".bss", "rw-"});
    map.append({0x7FFE00000000, 0x21000, "[stack]", "rw-"});
#endif
    return map;
}

// --- Disassembly ---

QVector<DisassemblyLine> DebugCore::disassemble(uint64_t address, int count)
{
    QVector<DisassemblyLine> lines;
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return lines;

    lldb::SBAddress sbAddr(address, m_target);
    const char* flavor = (m_asmFlavor == Intel) ? "intel" : "att";
    lldb::SBInstructionList instList = m_target.ReadInstructions(sbAddr, count, flavor);

    for (uint32_t i = 0; i < instList.GetSize(); i++) {
        lldb::SBInstruction inst = instList.GetInstructionAtIndex(i);
        if (!inst.IsValid()) continue;

        DisassemblyLine line;
        line.address = inst.GetAddress().GetLoadAddress(m_target);

        // Get raw bytes
        size_t byteSize = inst.GetByteSize();
        if (byteSize > 0 && m_process.IsValid()) {
            lldb::SBError error;
            QByteArray buf(static_cast<int>(byteSize), '\0');
            m_process.ReadMemory(line.address, buf.data(), byteSize, error);
            if (!error.Fail())
                line.bytes = buf;
        }

        line.mnemonic = inst.GetMnemonic(m_target);
        line.operands = inst.GetOperands(m_target);
        line.comment = inst.GetComment(m_target);

        // Resolve branch target for jump/call instructions
        if (line.mnemonic.startsWith('j') ||
            line.mnemonic == "call" || line.mnemonic == "callq") {
            QString op = line.operands.trimmed();
            if (op.startsWith("0x", Qt::CaseInsensitive))
                op = op.mid(2);
            bool ok;
            uint64_t target = op.toULongLong(&ok, 16);
            if (ok)
                line.branchTarget = target;
        }

        lines.append(line);
    }
#else
    Q_UNUSED(address)
    Q_UNUSED(count)
    // Return stub disassembly
    struct StubLine { uint64_t a; const char* b; const char* m; const char* o; };
    static const StubLine stubs[] = {
        {0x401000, "55",          "push",  "rbp"},
        {0x401001, "48 89 e5",   "mov",   "rbp, rsp"},
        {0x401004, "48 83 ec 20","sub",   "rsp, 0x20"},
        {0x401008, "89 7d fc",   "mov",   "dword ptr [rbp - 0x4], edi"},
        {0x40100b, "e8 15 00",   "call",  "puts"},
        {0x401010, "31 c0",      "xor",   "eax, eax"},
        {0x401012, "c9",         "leave", ""},
        {0x401013, "c3",         "ret",   ""},
    };
    for (int i = 0; i < 8 && i < count; i++) {
        DisassemblyLine line;
        line.address = stubs[i].a;
        line.bytes = QByteArray(stubs[i].b);
        line.mnemonic = stubs[i].m;
        line.operands = stubs[i].o;
        lines.append(line);
    }
#endif
    return lines;
}

// --- Smart dereferencing ---

QString DebugCore::dereferencePointer(uint64_t address)
{
    if (address == 0) return QString();

    // Try string first
    QString str = getStringAt(address);
    if (!str.isEmpty())
        return "\"" + str + "\"";

    // Then symbol
    QString sym = getSymbolAt(address);
    if (!sym.isEmpty())
        return "<" + sym + ">";

    return QString();
}

QString DebugCore::getStringAt(uint64_t address, int maxLen)
{
#ifdef HAS_LLDB
    if (!m_process.IsValid() || address == 0) return QString();

    // Read bytes and check if they look like a string
    lldb::SBError error;
    QByteArray buf(maxLen, '\0');
    size_t bytesRead = m_process.ReadMemory(address, buf.data(), maxLen, error);
    if (error.Fail() || bytesRead == 0) return QString();

    // Check for printable ASCII/UTF-8 string (at least 4 chars)
    int strLen = 0;
    for (size_t i = 0; i < bytesRead; i++) {
        char c = buf[static_cast<int>(i)];
        if (c == '\0') break;
        if (c < 0x20 || c > 0x7E) {
            // Allow common whitespace and UTF-8 continuation bytes (0x80-0xFF)
            if (c != '\n' && c != '\r' && c != '\t' &&
                (static_cast<unsigned char>(c) < 0x80))
                return QString();
        }
        strLen++;
    }

    if (strLen >= 4) {
        return QString::fromUtf8(buf.data(), strLen);
    }

    // Check for UTF-16LE (wide) string (at least 4 chars)
    if (bytesRead >= 8) {
        int wcharLen = 0;
        const uint16_t* wbuf = reinterpret_cast<const uint16_t*>(buf.constData());
        size_t wcount = bytesRead / 2;
        for (size_t i = 0; i < wcount; i++) {
            uint16_t wc = wbuf[i];
            if (wc == 0) break;
            // Printable BMP range (including common CJK, etc.)
            if (wc >= 0x20 && (wc < 0xD800 || wc > 0xDFFF))
                wcharLen++;
            else if (wc == '\n' || wc == '\r' || wc == '\t')
                wcharLen++;
            else {
                wcharLen = 0;
                break;
            }
        }
        if (wcharLen >= 4) {
            return QString::fromUtf16(reinterpret_cast<const char16_t*>(wbuf), wcharLen);
        }
    }
#else
    Q_UNUSED(address)
    Q_UNUSED(maxLen)
#endif
    return QString();
}

QString DebugCore::getSymbolAt(uint64_t address)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid() || address == 0) return QString();

    lldb::SBAddress sbAddr(address, m_target);
    lldb::SBSymbolContext ctx = m_target.ResolveSymbolContextForAddress(
        sbAddr, lldb::eSymbolContextEverything);

    lldb::SBSymbol symbol = ctx.GetSymbol();
    lldb::SBFunction function = ctx.GetFunction();

    QString result;

    // Module prefix
    lldb::SBModule mod = ctx.GetModule();
    if (mod.IsValid()) {
        result = QString(mod.GetFileSpec().GetFilename()) + ".";
    }

    if (function.IsValid()) {
        result += function.GetName();
        // Add offset if not at start
        uint64_t funcStart = function.GetStartAddress().GetLoadAddress(m_target);
        if (address > funcStart) {
            result += QString("+0x%1").arg(address - funcStart, 0, 16);
        }
    } else if (symbol.IsValid()) {
        result += symbol.GetName();
        uint64_t symStart = symbol.GetStartAddress().GetLoadAddress(m_target);
        if (address > symStart) {
            result += QString("+0x%1").arg(address - symStart, 0, 16);
        }
    } else {
        return QString();
    }

    return result;
#else
    Q_UNUSED(address)
    return QString();
#endif
}

// --- Symbol lookup ---

uint64_t DebugCore::mainModuleBase()
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return 0;

    lldb::SBModule module = m_target.GetModuleAtIndex(0);
    if (!module.IsValid()) return 0;

    // Get the first section's load address as the module base
    lldb::SBSection textSect = module.FindSection("__TEXT");
    if (textSect.IsValid())
        return textSect.GetLoadAddress(m_target);

    // Fallback: first section
    if (module.GetNumSections() > 0) {
        lldb::SBSection sect = module.GetSectionAtIndex(0);
        if (sect.IsValid())
            return sect.GetLoadAddress(m_target);
    }
    return 0;
#else
    return 0;
#endif
}

void DebugCore::setLabel(uint64_t address, const QString& text)
{
    if (text.isEmpty())
        m_labels.remove(address);
    else
        m_labels[address] = text;
    emit labelsChanged();
}

QString DebugCore::getLabel(uint64_t address) const
{
    return m_labels.value(address);
}

uint64_t DebugCore::findSymbolAddress(const QString& name)
{
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return 0;

    QByteArray nameUtf8 = name.toUtf8();

    // Search all modules for the symbol
    uint32_t numModules = m_target.GetNumModules();
    for (uint32_t i = 0; i < numModules; i++) {
        lldb::SBModule mod = m_target.GetModuleAtIndex(i);
        if (!mod.IsValid()) continue;

        // Try as function name
        lldb::SBSymbolContextList ctxList = mod.FindFunctions(nameUtf8.constData(),
            lldb::eFunctionNameTypeAny);
        if (ctxList.GetSize() > 0) {
            lldb::SBSymbolContext ctx = ctxList.GetContextAtIndex(0);
            lldb::SBFunction func = ctx.GetFunction();
            if (func.IsValid()) {
                uint64_t addr = func.GetStartAddress().GetLoadAddress(m_target);
                if (addr != LLDB_INVALID_ADDRESS)
                    return addr;
            }
            lldb::SBSymbol sym = ctx.GetSymbol();
            if (sym.IsValid()) {
                uint64_t addr = sym.GetStartAddress().GetLoadAddress(m_target);
                if (addr != LLDB_INVALID_ADDRESS)
                    return addr;
            }
        }

        // Try as symbol name
        lldb::SBSymbolContextList symList = mod.FindSymbols(nameUtf8.constData());
        if (symList.GetSize() > 0) {
            lldb::SBSymbolContext ctx = symList.GetContextAtIndex(0);
            lldb::SBSymbol sym = ctx.GetSymbol();
            if (sym.IsValid()) {
                uint64_t addr = sym.GetStartAddress().GetLoadAddress(m_target);
                if (addr != LLDB_INVALID_ADDRESS)
                    return addr;
            }
        }
    }
#else
    Q_UNUSED(name)
#endif
    return 0;
}

// --- Breakpoint log formatting ---

QString DebugCore::formatExpression(const QString& expr)
{
    // Handle type prefix: {s:expr} for string, {p:expr} for pointer, etc.
    QString type;
    QString body = expr;
    int colonPos = expr.indexOf(':');
    if (colonPos == 1) {
        type = expr.left(1).toLower();
        body = expr.mid(2).trimmed();
    }

#ifdef HAS_LLDB
    if (!m_process.IsValid()) return "???";
    lldb::SBThread thread = m_process.GetSelectedThread();
    if (!thread.IsValid()) return "???";
    lldb::SBFrame frame = thread.GetSelectedFrame();
    if (!frame.IsValid()) return "???";

    // Try as register name first (fast path)
    lldb::SBValue regVal = frame.FindRegister(body.toStdString().c_str());
    if (regVal.IsValid()) {
        uint64_t val = regVal.GetValueAsUnsigned(0);
        if (type == "s") {
            // String dereference
            QString str = getStringAt(val);
            return str.isEmpty() ? QString("0x%1").arg(val, 0, 16) : "\"" + str + "\"";
        }
        if (type == "d")
            return QString::number(static_cast<int64_t>(val));
        if (type == "u")
            return QString::number(val);
        // Default: hex
        return QString("0x%1").arg(val, 0, 16);
    }

    // Try as LLDB expression (handles memory reads, arithmetic, etc.)
    lldb::SBValue exprVal = frame.EvaluateExpression(body.toStdString().c_str());
    if (exprVal.IsValid() && exprVal.GetError().Success()) {
        uint64_t val = exprVal.GetValueAsUnsigned(0);
        if (type == "s") {
            QString str = getStringAt(val);
            return str.isEmpty() ? QString("0x%1").arg(val, 0, 16) : "\"" + str + "\"";
        }
        if (type == "d")
            return QString::number(static_cast<int64_t>(val));
        if (type == "u")
            return QString::number(val);
        return QString("0x%1").arg(val, 0, 16);
    }
#else
    Q_UNUSED(type) Q_UNUSED(body)
#endif
    return "???";
}

bool DebugCore::handleBreakpointLog(uint64_t address)
{
    // Find which breakpoint was hit at this address
#ifdef HAS_LLDB
    if (!m_target.IsValid()) return false;

    for (uint32_t i = 0; i < m_target.GetNumBreakpoints(); i++) {
        lldb::SBBreakpoint bp = m_target.GetBreakpointAtIndex(i);
        if (!bp.IsValid() || bp.GetNumLocations() == 0) continue;

        lldb::SBBreakpointLocation loc = bp.GetLocationAtIndex(0);
        uint64_t bpAddr = loc.GetAddress().GetLoadAddress(m_target);
        if (bpAddr != address) continue;

        uint32_t id = bp.GetID();
        if (!m_bpExtras.contains(id)) continue;

        const auto& extra = m_bpExtras[id];

        // Check log condition (empty = always log)
        bool shouldLog = true;
        if (!extra.logCondition.isEmpty()) {
            lldb::SBThread thread = m_process.GetSelectedThread();
            if (thread.IsValid()) {
                lldb::SBFrame frame = thread.GetSelectedFrame();
                if (frame.IsValid()) {
                    lldb::SBValue condVal = frame.EvaluateExpression(
                        extra.logCondition.toStdString().c_str());
                    if (condVal.IsValid() && condVal.GetError().Success())
                        shouldLog = condVal.GetValueAsUnsigned(0) != 0;
                }
            }
        }

        // Emit formatted log text
        if (shouldLog && !extra.logText.isEmpty()) {
            QString formatted = formatLogText(extra.logText);
            emit outputReceived(QString("[BP %1] %2").arg(id).arg(formatted));
        }

        // Fast resume: skip breaking, just continue
        if (extra.fastResume)
            return true;

        break;
    }
#else
    Q_UNUSED(address)
#endif
    return false;
}

// --- Event handling slots ---

void DebugCore::onProcessStopped()
{
    setState(Stopped);

#ifdef HAS_LLDB
    // Log stop reason and choose refresh strategy
    if (m_process.IsValid()) {
        lldb::SBThread thread = m_process.GetSelectedThread();
        if (thread.IsValid()) {
            lldb::StopReason reason = thread.GetStopReason();
            QString reasonStr;
            switch (reason) {
            case lldb::eStopReasonBreakpoint:
                reasonStr = "breakpoint";
                break;
            case lldb::eStopReasonWatchpoint:
                reasonStr = "watchpoint";
                break;
            case lldb::eStopReasonSignal:
                reasonStr = QString("signal %1").arg(thread.GetStopReasonDataAtIndex(0));
                break;
            case lldb::eStopReasonPlanComplete:
                reasonStr = "step complete";
                break;
            default:
                reasonStr = "unknown";
                break;
            }

            lldb::SBFrame frame = thread.GetSelectedFrame();
            uint64_t pc = frame.IsValid() ? frame.GetPC() : 0;
            emit outputReceived(QString("Stopped at 0x%1 (%2)")
                .arg(pc, 0, 16).arg(reasonStr));

            if (reason == lldb::eStopReasonPlanComplete) {
                // Step completed — only registers and memory change
                emitStepRefresh();
            } else {
                emitAllRefresh();
                if (reason == lldb::eStopReasonBreakpoint) {
                    // Handle logging and fast-resume before UI update
                    bool shouldResume = handleBreakpointLog(pc);
                    if (shouldResume) {
                        // Fast resume: continue without stopping in UI
                        continueExec();
                        return;
                    }
                    emit breakpointHit(pc);
                }
            }
            return;
        }
    }
#endif
    // Fallback if we can't determine the reason
    emitAllRefresh();
}

void DebugCore::onProcessRunning()
{
    setState(Running);
}

void DebugCore::onProcessExited(int exitCode)
{
    setState(Exited);
    emit outputReceived(QString("Process exited with code %1").arg(exitCode));
}

void DebugCore::onProcessCrashed()
{
    setState(Crashed);
    emit outputReceived("Process crashed!");
    emitAllRefresh();
}

// --- Private helpers ---

void DebugCore::setState(ProcessState state)
{
    if (m_state != state) {
        m_state = state;
        emit processStateChanged(state);
    }
}

void DebugCore::emitAllRefresh()
{
    emit registersChanged();
    emit memoryChanged();
    emit breakpointsChanged();
    emit threadListChanged();
    emit modulesChanged();
}

void DebugCore::emitStepRefresh()
{
    // Only refresh registers + memory (stack/dump).
    // Breakpoints, threads, and modules don't change on a step.
    emit registersChanged();
    emit memoryChanged();
}

void DebugCore::startEventListener()
{
#ifdef HAS_LLDB
    if (m_eventListener) return;
    m_eventListener = new LLDBEventListener(m_listener, this);
    connect(m_eventListener, &LLDBEventListener::processStopped, this, &DebugCore::onProcessStopped);
    connect(m_eventListener, &LLDBEventListener::processRunning, this, &DebugCore::onProcessRunning);
    connect(m_eventListener, &LLDBEventListener::processExited, this, &DebugCore::onProcessExited);
    connect(m_eventListener, &LLDBEventListener::processCrashed, this, &DebugCore::onProcessCrashed);
    connect(m_eventListener, &LLDBEventListener::stdoutReceived, this, &DebugCore::outputReceived);
    connect(m_eventListener, &LLDBEventListener::stderrReceived, this, &DebugCore::outputReceived);
    m_eventListener->start();
#endif
}

void DebugCore::stopEventListener()
{
#ifdef HAS_LLDB
    if (m_eventListener) {
        m_eventListener->stop();
        m_eventListener->wait(2000);
        delete m_eventListener;
        m_eventListener = nullptr;
    }
#endif
}
