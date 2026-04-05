#include "CommandLineEdit.h"
#include "core/DebugCore.h"
#include "common/Configuration.h"
#include "gui/widgets/CPUDump.h"
#include "gui/widgets/LogView.h"

#include <QKeyEvent>
#include <QFile>
#include <QFileInfo>

CommandLineEdit::CommandLineEdit(DebugCore* debugCore, QWidget* parent)
    : QLineEdit(parent)
    , m_debugCore(debugCore)
{
    setPlaceholderText("Command (e.g., dump rdi, savedata :memdump: rsp 0x100)");
    setFont(ConfigFont("Disassembly"));

    QColor bg = ConfigColor("ChromeSurfaceColor");
    QColor fg = ConfigColor("DisassemblyTextColor");
    QColor border = ConfigColor("ChromeBorderColor");
    QColor accent = ConfigColor("ChromeAccentColor");

    setStyleSheet(QString(
        "QLineEdit { background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 0; padding: 4px 8px; font-size: 12px; }"
        "QLineEdit:focus { border: 1px solid %4; }"
    ).arg(bg.name(), fg.name(), border.name(), accent.name()));

    connect(this, &QLineEdit::returnPressed, this, [this]() {
        QString cmd = text().trimmed();
        if (!cmd.isEmpty()) {
            m_history.prepend(cmd);
            m_historyIndex = -1;
            executeCommand(cmd);
            clear();
        }
    });
}

void CommandLineEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Up) {
        if (!m_history.isEmpty() && m_historyIndex < m_history.size() - 1) {
            m_historyIndex++;
            setText(m_history[m_historyIndex]);
        }
        return;
    } else if (event->key() == Qt::Key_Down) {
        if (m_historyIndex > 0) {
            m_historyIndex--;
            setText(m_history[m_historyIndex]);
        } else if (m_historyIndex == 0) {
            m_historyIndex = -1;
            clear();
        }
        return;
    } else if (event->key() == Qt::Key_Escape) {
        clear();
        m_historyIndex = -1;
        return;
    }
    QLineEdit::keyPressEvent(event);
}

void CommandLineEdit::executeCommand(const QString& cmd)
{
    // Parse command and arguments
    // x64dbg syntax: command arg1, arg2, arg3
    // We also accept space-separated for convenience
    QString normalized = cmd.trimmed();

    // Split on first space to get command name
    int spaceIdx = normalized.indexOf(' ');
    QString cmdName = (spaceIdx >= 0) ? normalized.left(spaceIdx).toLower() : normalized.toLower();
    QString argStr = (spaceIdx >= 0) ? normalized.mid(spaceIdx + 1).trimmed() : QString();

    // Split arguments by comma or space
    QStringList args;
    if (!argStr.isEmpty()) {
        // Try comma first, then space
        if (argStr.contains(','))
            args = argStr.split(',', Qt::SkipEmptyParts);
        else
            args = argStr.split(' ', Qt::SkipEmptyParts);
        for (auto& a : args)
            a = a.trimmed();
    }

    auto log = [this](const QString& msg) {
        emit commandOutput(msg);
    };

    // ---- dump / d ----
    if (cmdName == "dump" || cmdName == "d") {
        if (args.isEmpty()) {
            log("[cmd] Usage: dump <address/expr> [dump_index 1-5]");
            return;
        }
        bool ok = false;
        uint64_t addr = evaluateExpression(args[0], &ok);
        if (!ok) {
            log(QString("[cmd] Invalid expression: %1").arg(args[0]));
            return;
        }
        // Optional dump tab index
        int tabIdx = 0; // default: current
        if (args.size() >= 2) {
            int idx = args[1].toInt(&ok);
            if (ok && idx >= 1 && idx <= 5)
                tabIdx = idx - 1;
        }
        if (m_dump) {
            m_dump->goToAddress(addr);
            log(QString("[cmd] Dump at 0x%1").arg(addr, 0, 16));
        } else {
            log("[cmd] Dump widget not available");
        }
        return;
    }

    // ---- savedata ----
    if (cmdName == "savedata") {
        if (args.size() < 3) {
            log("[cmd] Usage: savedata <filename> <address> <size>");
            log("[cmd]   savedata :memdump: rsp 0x100");
            return;
        }
        QString filename = args[0];
        bool okAddr = false, okSize = false;
        uint64_t addr = evaluateExpression(args[1], &okAddr);
        uint64_t size = evaluateExpression(args[2], &okSize);
        if (!okAddr) {
            log(QString("[cmd] Invalid address expression: %1").arg(args[1]));
            return;
        }
        if (!okSize || size == 0) {
            log(QString("[cmd] Invalid size: %1").arg(args[2]));
            return;
        }

        // :memdump: generates auto filename
        if (filename == ":memdump:") {
            filename = QString("memdump_0x%1_%2.bin")
                .arg(addr, 0, 16)
                .arg(size);
        }

        QByteArray data = m_debugCore->readMemory(addr, static_cast<size_t>(size));
        if (data.isEmpty()) {
            log(QString("[cmd] Failed to read memory at 0x%1").arg(addr, 0, 16));
            return;
        }

        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            log(QString("[cmd] Saved %1 bytes to %2").arg(data.size()).arg(QFileInfo(filename).absoluteFilePath()));
        } else {
            log(QString("[cmd] Failed to write file: %1").arg(filename));
        }
        return;
    }

    // ---- help ----
    if (cmdName == "help" || cmdName == "?") {
        log("[cmd] Available commands:");
        log("[cmd]   dump/d <addr> [tab 1-5]  — Navigate dump to address");
        log("[cmd]   savedata <file> <addr> <size> — Save memory to file");
        log("[cmd]   help/?                    — Show this help");
        return;
    }

    log(QString("[cmd] Unknown command: %1 (type 'help' for available commands)").arg(cmdName));
}

uint64_t CommandLineEdit::evaluateExpression(const QString& expr, bool* ok)
{
    *ok = false;
    QString s = expr.trimmed();
    if (s.isEmpty())
        return 0;

    // Split on + or - keeping operator
    QVector<QPair<QChar, QString>> terms;
    QString current;
    QChar currentSign = '+';

    for (int i = 0; i < s.size(); i++) {
        QChar c = s[i];
        if ((c == '+' || c == '-') && i > 0) {
            if (!current.isEmpty()) {
                terms.append({currentSign, current.trimmed()});
                current.clear();
            }
            currentSign = c;
        } else {
            current += c;
        }
    }
    if (!current.trimmed().isEmpty())
        terms.append({currentSign, current.trimmed()});

    if (terms.isEmpty())
        return 0;

    uint64_t result = 0;
    for (const auto& term : terms) {
        bool termOk = false;
        uint64_t val = 0;
        QString t = term.second;

        // Try hex (0x...)
        if (t.startsWith("0x", Qt::CaseInsensitive))
            val = t.mid(2).toULongLong(&termOk, 16);

        // Try decimal
        if (!termOk)
            val = t.toULongLong(&termOk, 10);

        // Try plain hex
        if (!termOk)
            val = t.toULongLong(&termOk, 16);

        // Try register name
        if (!termOk) {
            auto regs = m_debugCore->getRegisters();
            for (const auto& reg : regs) {
                if (reg.name.compare(t, Qt::CaseInsensitive) == 0) {
                    val = reg.value;
                    termOk = true;
                    break;
                }
            }
        }

        // Try symbol
        if (!termOk) {
            uint64_t symAddr = m_debugCore->findSymbolAddress(t);
            if (symAddr != 0) {
                val = symAddr;
                termOk = true;
            }
        }

        if (!termOk)
            return 0;

        if (term.first == '+')
            result += val;
        else
            result -= val;
    }

    *ok = true;
    return result;
}
