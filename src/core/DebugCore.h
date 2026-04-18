#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPair>
#include <QMap>
#include <QSet>
#include <QThread>
#include <QMutex>
#include <cstdint>

#ifdef HAS_LLDB
#include <lldb/API/LLDB.h>
#endif

struct DisassemblyLine {
    uint64_t address;
    QByteArray bytes;
    QString mnemonic;
    QString operands;
    QString comment;
    uint64_t branchTarget = 0;  // resolved destination for jmp/jcc/call
};

struct RegisterInfo {
    QString name;
    uint64_t value;
    bool modified; // changed since last stop
};

struct StackEntry {
    uint64_t address;
    uint64_t value;
    QString comment; // dereferenced string/symbol if pointer
};

struct BreakpointInfo {
    uint32_t id;
    uint64_t address;
    QString module;
    bool enabled;
    bool isHardware;
    QString condition;       // break condition (LLDB expression)
    uint32_t hitCount;
    QString logText;         // log message with {register} placeholders
    QString logCondition;    // expression: log only when true (empty = always)
    bool fastResume = false; // skip break, just log and continue
    // Memory dump on hit
    QString dumpAddress;     // expression for start address (e.g., "rsi")
    QString dumpSize;        // expression for byte count (e.g., "rdx")
    QString dumpFilename;    // filename pattern with {HitCount}, {register} support
};

struct MemoryRegion {
    uint64_t base;
    uint64_t size;
    QString name;
    QString permissions; // rwx
};

struct ThreadInfo {
    uint32_t id;
    uint64_t pc;
    QString name;
    bool isCurrent;
};

struct CallStackEntry {
    uint32_t index;
    uint64_t address;
    uint64_t returnAddress;
    QString module;
    QString function;
};

#ifdef HAS_LLDB
// Background thread that polls LLDB's event listener
class LLDBEventListener : public QThread
{
    Q_OBJECT

public:
    explicit LLDBEventListener(lldb::SBListener& listener, QObject* parent = nullptr);
    void stop();

signals:
    void processStopped();
    void processRunning();
    void processExited(int exitCode);
    void processCrashed();
    void stdoutReceived(const QString& text);
    void stderrReceived(const QString& text);

protected:
    void run() override;

private:
    lldb::SBListener& m_listener;
    bool m_running = true;
};
#endif

class DebugCore : public QObject
{
    Q_OBJECT

public:
    enum ProcessState {
        Unloaded,
        Stopped,
        Running,
        Exited,
        Crashed
    };
    Q_ENUM(ProcessState)

    explicit DebugCore(QObject* parent = nullptr);
    ~DebugCore();

    // --- Session management ---
    bool startDebug(const QString& path, const QStringList& args = {}, const QString& arch = {});
    static QStringList detectArchitectures(const QString& path);
    bool attach(int pid);
    void detach();
    void stop();
    void restart();

    // --- Execution control ---
    void continueExec();
    void pause();
    void stepInto();
    void stepOver();
    void stepOut();
    void runToCursor(uint64_t address);

    // --- Breakpoints ---
    bool addBreakpoint(uint64_t address);
    bool addHardwareBreakpoint(uint64_t address);
    bool addConditionalBreakpoint(uint64_t address, const QString& condition);
    bool removeBreakpoint(uint64_t address);
    bool removeBreakpointById(uint32_t id);
    bool toggleBreakpoint(uint64_t address);
    QVector<BreakpointInfo> getBreakpoints() const;

    // --- Breakpoint properties (log, conditions) ---
    void setBreakpointLogText(uint32_t id, const QString& logText);
    void setBreakpointLogCondition(uint32_t id, const QString& logCondition);
    void setBreakpointCondition(uint32_t id, const QString& condition);
    void setBreakpointFastResume(uint32_t id, bool fastResume);
    void setBreakpointDump(uint32_t id, const QString& address, const QString& size, const QString& filename);
    QString formatLogText(const QString& logText);  // expand {register} placeholders

    // --- Data accessors ---
    ProcessState processState() const { return m_state; }
    uint64_t currentPC();
    QVector<RegisterInfo> getRegisters();
    QByteArray readMemory(uint64_t address, size_t size);
    bool writeMemory(uint64_t address, const QByteArray& data);
    QVector<StackEntry> getStackEntries(int count = 32);
    QVector<ThreadInfo> getThreads();
    QVector<CallStackEntry> getCallStack();
    QVector<MemoryRegion> getMemoryMap();

    // --- Disassembly ---
    QVector<DisassemblyLine> disassemble(uint64_t address, int count = 64);

    // --- Smart dereferencing ---
    QString dereferencePointer(uint64_t address);
    QString getStringAt(uint64_t address, int maxLen = 256);
    QString getSymbolAt(uint64_t address);

    // --- Command execution (LLDB commands + Python via 'script') ---
    bool executeCommand(const QString& command, QString& output, QString& error);
    bool executeScript(const QString& pythonCode, QString& output, QString& error);

    // --- Symbol lookup ---
    uint64_t findSymbolAddress(const QString& name);

    // --- Disassembly flavor ---
    enum AsmFlavor { Intel, ATT };
    AsmFlavor asmFlavor() const { return m_asmFlavor; }
    void setAsmFlavor(AsmFlavor flavor);

    // --- Target info ---
    QString targetPath() const { return m_targetPath; }
    uint64_t mainModuleBase();

    // --- User labels ---
    void setLabel(uint64_t address, const QString& text);
    QString getLabel(uint64_t address) const;

signals:
    void labelsChanged();
    void processStateChanged(DebugCore::ProcessState state);
    void registersChanged();
    void memoryChanged();
    void breakpointHit(uint64_t address);
    void breakpointsChanged();
    void threadListChanged();
    void modulesChanged();
    void outputReceived(const QString& message);
    void asmFlavorChanged();

private slots:
    void onProcessStopped();
    void onProcessRunning();
    void onProcessExited(int exitCode);
    void onProcessCrashed();

private:
    void setState(ProcessState state);
    void emitAllRefresh();
    void emitStepRefresh();   // lightweight refresh for step completion
    void startEventListener();
    void stopEventListener();
    QString formatExpression(const QString& expr);  // evaluate single {expr}
    bool handleBreakpointLog(uint64_t address);     // log + fast-resume; returns true to resume

    ProcessState m_state = Unloaded;
    AsmFlavor m_asmFlavor = Intel;
    QString m_targetPath;
    QStringList m_targetArgs;
    QString m_targetArch;

    // Previous register values for change detection
    QMap<QString, uint64_t> m_prevRegisters;

    // User-defined labels (address → name)
    QMap<uint64_t, QString> m_labels;

    // Per-breakpoint extra data (keyed by LLDB breakpoint ID)
    struct BpExtra {
        QString logText;
        QString logCondition;
        bool fastResume = false;
        // Memory dump on hit
        QString dumpAddress;   // expression for start address (e.g., "rsi")
        QString dumpSize;      // expression for byte count (e.g., "rdx")
        QString dumpFilename;  // pattern: supports {HitCount}, {rax}, etc.
    };
    QMap<uint32_t, BpExtra> m_bpExtras;

#ifdef HAS_LLDB
    lldb::SBDebugger m_debugger;
    lldb::SBTarget m_target;
    lldb::SBProcess m_process;
    lldb::SBListener m_listener;
    LLDBEventListener* m_eventListener = nullptr;
#endif
};
