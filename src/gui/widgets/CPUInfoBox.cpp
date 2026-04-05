#include "CPUInfoBox.h"
#include "common/Configuration.h"

#include <QHeaderView>
#include <QRegularExpression>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>

CPUInfoBox::CPUInfoBox(DebugCore* debugCore, QWidget* parent)
    : QTableWidget(parent)
    , m_debugCore(debugCore)
{
    setupLayout();
    applyStyle();
    setupContextMenu();
}

void CPUInfoBox::setupLayout()
{
    setColumnCount(1);
    setRowCount(4);
    horizontalHeader()->setVisible(false);
    verticalHeader()->setVisible(false);
    horizontalHeader()->setStretchLastSection(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setShowGrid(false);
    setMaximumHeight(80);
    verticalHeader()->setDefaultSectionSize(18);
}

void CPUInfoBox::applyStyle()
{
    QFont font = ConfigFont("InfoBox");
    setFont(font);

    QColor bg = ConfigColor("InfoBoxBackgroundColor");
    QColor fg = ConfigColor("InfoBoxTextColor");
    QColor grid = ConfigColor("TableGridColor");

    setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: none; border-top: 1px solid %3; outline: none; }"
        "QTableWidget::item { padding: 0 4px; }"
    ).arg(bg.name(), fg.name(), grid.name()));
}

void CPUInfoBox::setupContextMenu()
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        auto* copyAction = menu.addAction("Copy");
        copyAction->setShortcut(QKeySequence::Copy);
        connect(copyAction, &QAction::triggered, this, [this]() {
            auto* it = currentItem();
            if (it && !it->text().isEmpty())
                QApplication::clipboard()->setText(it->text());
        });
        menu.exec(viewport()->mapToGlobal(pos));
    });
}

void CPUInfoBox::setInfoLine(int row, const QString& text, const QColor& color)
{
    if (row < 0 || row >= rowCount()) return;
    auto* it = new QTableWidgetItem(text);
    if (color.isValid())
        it->setForeground(color);
    else
        it->setForeground(ConfigColor("InfoBoxTextColor"));
    setItem(row, 0, it);
}

// ============================================================
//  Main update — called when user selects an instruction
// ============================================================
void CPUInfoBox::updateInfo(uint64_t address)
{
    // Clear all rows
    for (int i = 0; i < 4; i++)
        setInfoLine(i, "");

    if (address == 0) return;

    // Cache current register values
    m_regCache.clear();
    auto regs = m_debugCore->getRegisters();
    for (const auto& reg : regs)
        m_regCache[reg.name.toLower()] = reg.value;

    // Disassemble the selected instruction
    auto lines = m_debugCore->disassemble(address, 1);

    QString mnemonic;
    QString operands;
    if (!lines.isEmpty()) {
        mnemonic = lines[0].mnemonic.toLower();
        operands = lines[0].operands;

        // Fix RIP-relative addressing: RIP for this instruction should be
        // the address of the NEXT instruction, not the current execution RIP.
        uint64_t nextInstrAddr = address + static_cast<uint64_t>(lines[0].bytes.size());
        m_regCache["rip"] = nextInstrAddr;
    }


    QColor addrColor = ConfigColor("DisassemblyAddressColor");
    QColor strColor = ConfigColor("StringReferenceColor");
    QColor symColor = ConfigColor("SymbolReferenceColor");
    QColor defaultColor = ConfigColor("InfoBoxTextColor");
    QColor jumpTakenColor = ConfigColor("DisassemblyRegistersColor"); // green
    QColor jumpNotTakenColor = ConfigColor("BreakpointColor");        // red

    int nextRow = 0;

    // --- Row 0: Conditional jump analysis ---
    if (isConditionalJump(mnemonic)) {
        uint64_t rflags = m_regCache.value("rflags", 0);
        bool taken = isJumpTaken(mnemonic, rflags);
        if (taken) {
            // Also show the target
            uint64_t target = resolveOperandValue(operands.trimmed());
            QString targetInfo;
            if (target != 0) {
                targetInfo = formatAddress(target);
            }
            setInfoLine(0, QString("Jump is taken%1")
                .arg(targetInfo.isEmpty() ? "" : " → " + targetInfo), jumpTakenColor);
        } else {
            setInfoLine(0, "Jump is NOT taken", jumpNotTakenColor);
        }
        nextRow = 1;
    }
    // Unconditional jumps: show target
    else if (mnemonic == "jmp" || mnemonic == "jmpq") {
        uint64_t target = resolveOperandValue(operands.trimmed());
        if (target != 0) {
            setInfoLine(0, "Jump target: " + formatAddress(target), symColor);
        }
        nextRow = 1;
    }
    // Call instructions: show target
    else if (mnemonic == "call" || mnemonic == "callq") {
        uint64_t target = resolveOperandValue(operands.trimmed());
        if (target != 0) {
            setInfoLine(0, "Call target: " + formatAddress(target), symColor);
        }
        nextRow = 1;
    }

    // --- Operand analysis ---
    // Split operands by comma
    QStringList ops = operands.split(',');
    for (int i = 0; i < ops.size() && nextRow < 3; i++) {
        QString op = ops[i].trimmed();
        if (op.isEmpty()) continue;

        if (isMemoryOperand(op)) {
            // Memory operand: compute effective address and dereference
            uint64_t effAddr = resolveOperandValue(op);

            if (effAddr != 0) {
                // Always check if the effective address itself holds a string first
                QString strAtAddr = m_debugCore->getStringAt(effAddr);
                if (!strAtAddr.isEmpty()) {
                    QString line = QString("[0x%1]=\"%2\"")
                        .arg(effAddr, 0, 16).arg(strAtAddr.left(64));
                    setInfoLine(nextRow++, line, strColor);
                } else {
                    // Read memory as pointer and try to dereference
                    uint64_t memValue = readPointer(effAddr);
                    QString deref = formatDerefChain(memValue);

                    QString line = QString("[0x%1]=").arg(effAddr, 0, 16);
                    if (deref.isEmpty())
                        line += QString("0x%1").arg(memValue, 0, 16);
                    else
                        line += deref;

                    setInfoLine(nextRow++, line, defaultColor);
                }
            } else {
                setInfoLine(nextRow++, QString("%1=???").arg(op), addrColor);
            }
        } else {
            // Register or immediate operand: always show resolved value
            uint64_t val = resolveOperandValue(op);
            if (val != 0) {
                // Clean up display name (strip AT&T %)
                QString displayName = op;
                displayName.remove('%');
                QString line = QString("%1=%2").arg(displayName, formatAddress(val));
                // Try dereference chain for extra info
                QString str = m_debugCore->getStringAt(val);
                if (!str.isEmpty()) {
                    line = QString("%1=0x%2 \"%3\"").arg(displayName)
                        .arg(val, 0, 16).arg(str.left(64));
                    setInfoLine(nextRow++, line, strColor);
                } else {
                    // Try one level of pointer dereference
                    uint64_t derefVal = readPointer(val);
                    if (derefVal != 0 && derefVal != val) {
                        QString derefStr = m_debugCore->getStringAt(derefVal);
                        if (!derefStr.isEmpty()) {
                            line += QString(" → \"%1\"").arg(derefStr.left(64));
                        } else {
                            QString derefSym = m_debugCore->getSymbolAt(derefVal);
                            if (!derefSym.isEmpty())
                                line += QString(" → <%1>").arg(derefSym);
                        }
                    }
                    setInfoLine(nextRow++, line, defaultColor);
                }
            }
        }
    }

    // --- Last row: Address info line (module:$RVA <symbol>) ---
    QString addrInfo;
    // Module info
    QString modInfo = getModuleAndOffset(address);
    if (!modInfo.isEmpty())
        addrInfo = modInfo;
    else
        addrInfo = QString("0x%1").arg(address, 0, 16);

    // Symbol/label
    QString sym = m_debugCore->getSymbolAt(address);
    if (!sym.isEmpty())
        addrInfo += " <" + sym + ">";

    setInfoLine(3, addrInfo, addrColor);

    // Remove duplicate lines
    for (int i = 1; i < 3; i++) {
        if (item(i, 0) && item(i - 1, 0) &&
            item(i, 0)->text() == item(i - 1, 0)->text())
            setInfoLine(i, "");
    }
}

// ============================================================
//  Helpers
// ============================================================

bool CPUInfoBox::isConditionalJump(const QString& mnemonic) const
{
    // All x86 conditional jumps (Jcc)
    static const QStringList cjumps = {
        "jo", "jno", "jb", "jnae", "jc", "jnb", "jae", "jnc",
        "jz", "je", "jnz", "jne", "jbe", "jna", "jnbe", "ja",
        "js", "jns", "jp", "jpe", "jnp", "jpo",
        "jl", "jnge", "jnl", "jge", "jle", "jng", "jnle", "jg",
        "jcxz", "jecxz", "jrcxz"
    };
    return cjumps.contains(mnemonic);
}

bool CPUInfoBox::isJumpTaken(const QString& mnemonic, uint64_t rflags) const
{
    // RFLAGS bits
    bool CF = (rflags >> 0) & 1;  // Carry
    bool PF = (rflags >> 2) & 1;  // Parity
    bool ZF = (rflags >> 6) & 1;  // Zero
    bool SF = (rflags >> 7) & 1;  // Sign
    bool OF = (rflags >> 11) & 1; // Overflow

    if (mnemonic == "jo")                              return OF;
    if (mnemonic == "jno")                             return !OF;
    if (mnemonic == "jb" || mnemonic == "jnae" || mnemonic == "jc")  return CF;
    if (mnemonic == "jnb" || mnemonic == "jae" || mnemonic == "jnc") return !CF;
    if (mnemonic == "jz" || mnemonic == "je")          return ZF;
    if (mnemonic == "jnz" || mnemonic == "jne")        return !ZF;
    if (mnemonic == "jbe" || mnemonic == "jna")        return CF || ZF;
    if (mnemonic == "jnbe" || mnemonic == "ja")        return !CF && !ZF;
    if (mnemonic == "js")                              return SF;
    if (mnemonic == "jns")                             return !SF;
    if (mnemonic == "jp" || mnemonic == "jpe")         return PF;
    if (mnemonic == "jnp" || mnemonic == "jpo")        return !PF;
    if (mnemonic == "jl" || mnemonic == "jnge")        return SF != OF;
    if (mnemonic == "jnl" || mnemonic == "jge")        return SF == OF;
    if (mnemonic == "jle" || mnemonic == "jng")        return ZF || (SF != OF);
    if (mnemonic == "jnle" || mnemonic == "jg")        return !ZF && (SF == OF);

    return false;
}

uint64_t CPUInfoBox::resolveOperandValue(const QString& operand) const
{
    QString op = operand.trimmed();
    if (op.isEmpty()) return 0;

    // Strip AT&T % prefix from registers (LLDB uses AT&T syntax)
    op.remove('%');

    // Strip size prefixes: "qword ptr", "dword ptr", etc. (Intel syntax)
    static QRegularExpression sizeRe("^(byte|word|dword|qword|xmmword|ymmword)\\s+ptr\\s+",
                                     QRegularExpression::CaseInsensitiveOption);
    op = op.remove(sizeRe).trimmed();

    // Strip segment prefix: "gs:", "fs:", etc.
    static QRegularExpression segRe("^[cdefgs]s:",
                                    QRegularExpression::CaseInsensitiveOption);
    op = op.remove(segRe).trimmed();

    // Strip Intel-style brackets: [expr]
    if (op.startsWith('[') && op.endsWith(']'))
        op = op.mid(1, op.length() - 2).trimmed();

    // Handle AT&T-style memory: disp(%base, %index, scale) or (%base) or disp(%base)
    // e.g., "0x10(%rbp)", "(%rsp)", "(%rax,%rbx,4)", "-0x4(%rbp)"
    static QRegularExpression attMemRe(
        "^([^(]*)\\(([^)]+)\\)$");
    auto attMatch = attMemRe.match(op);
    if (attMatch.hasMatch()) {
        QString disp = attMatch.captured(1).trimmed();
        QString inside = attMatch.captured(2).trimmed();

        // Parse base, index, scale from inside parens
        QStringList parts = inside.split(',');
        uint64_t result = 0;

        // Base register
        if (!parts.isEmpty() && !parts[0].trimmed().isEmpty()) {
            auto it = m_regCache.find(parts[0].trimmed().toLower());
            if (it != m_regCache.end())
                result = it.value();
        }

        // Index register * scale
        if (parts.size() >= 2 && !parts[1].trimmed().isEmpty()) {
            auto it = m_regCache.find(parts[1].trimmed().toLower());
            if (it != m_regCache.end()) {
                uint64_t scale = 1;
                if (parts.size() >= 3) {
                    bool ok;
                    scale = parts[2].trimmed().toULongLong(&ok);
                    if (!ok) scale = 1;
                }
                result += it.value() * scale;
            }
        }

        // Displacement
        if (!disp.isEmpty()) {
            bool neg = disp.startsWith('-');
            QString d = neg ? disp.mid(1) : disp;
            bool ok = false;
            uint64_t dispVal = 0;
            if (d.startsWith("0x", Qt::CaseInsensitive))
                dispVal = d.mid(2).toULongLong(&ok, 16);
            else
                dispVal = d.toULongLong(&ok, 10);
            if (!ok)
                dispVal = d.toULongLong(&ok, 16);
            if (ok) {
                if (neg) result -= dispVal;
                else result += dispVal;
            }
        }

        return result;
    }

    // Try as hex immediate (0x...)
    if (op.startsWith("0x", Qt::CaseInsensitive)) {
        bool ok = false;
        uint64_t val = op.mid(2).toULongLong(&ok, 16);
        if (ok) return val;
    }

    // Try as negative hex (-0x...)
    if (op.startsWith("-0x", Qt::CaseInsensitive)) {
        bool ok = false;
        uint64_t val = op.mid(3).toULongLong(&ok, 16);
        if (ok) return static_cast<uint64_t>(-static_cast<int64_t>(val));
    }

    // Try as register
    {
        auto it = m_regCache.find(op.toLower());
        if (it != m_regCache.end())
            return it.value();
    }

    // Try as plain hex (e.g., "401000") — only if 4+ chars to avoid matching small decimals
    {
        bool ok = false;
        uint64_t val = op.toULongLong(&ok, 16);
        if (ok && op.length() >= 4) return val;
    }

    // Handle expressions with + and - (e.g., "rbp - 0x4", "rsi+8")
    QVector<QPair<QChar, QString>> terms;
    QString current;
    QChar currentSign = '+';
    for (int i = 0; i < op.size(); i++) {
        QChar c = op[i];
        if ((c == '+' || c == '-') && i > 0) {
            if (!current.trimmed().isEmpty()) {
                terms.append({currentSign, current.trimmed()});
                current.clear();
            }
            currentSign = c;
        } else if (c != ' ') {
            current += c;
        }
    }
    if (!current.trimmed().isEmpty())
        terms.append({currentSign, current.trimmed()});

    if (terms.size() > 1) {
        uint64_t result = 0;
        bool allOk = true;
        for (const auto& term : terms) {
            uint64_t val = 0;
            bool ok = false;
            QString t = term.second;

            // Try register
            auto it = m_regCache.find(t.toLower());
            if (it != m_regCache.end()) {
                val = it.value();
                ok = true;
            }
            // Try hex
            if (!ok && t.startsWith("0x", Qt::CaseInsensitive))
                val = t.mid(2).toULongLong(&ok, 16);
            // Try decimal
            if (!ok)
                val = t.toULongLong(&ok, 10);
            // Try plain hex
            if (!ok)
                val = t.toULongLong(&ok, 16);

            if (!ok) { allOk = false; break; }

            if (term.first == '+')
                result += val;
            else
                result -= val;
        }
        if (allOk) return result;
    }

    // Try as symbol
    uint64_t symAddr = m_debugCore->findSymbolAddress(op);
    if (symAddr != 0) return symAddr;

    return 0;
}

QString CPUInfoBox::formatAddress(uint64_t address) const
{
    QString result = QString("0x%1").arg(address, 0, 16);

    QString sym = m_debugCore->getSymbolAt(address);
    if (!sym.isEmpty())
        result += " <" + sym + ">";

    return result;
}

QString CPUInfoBox::formatDerefChain(uint64_t address) const
{
    if (address == 0) return QString();

    QString result = QString("0x%1").arg(address, 0, 16);

    // Level 1: symbol or string at address
    QString str = m_debugCore->getStringAt(address);
    if (!str.isEmpty()) {
        return result + " \"" + str.left(64) + "\"";
    }

    QString sym = m_debugCore->getSymbolAt(address);
    if (!sym.isEmpty()) {
        result += " <" + sym + ">";
    }

    // Level 2: if it looks like a pointer, try one more deref
    uint64_t deref = readPointer(address);
    if (deref != 0 && deref != address) {
        QString str2 = m_debugCore->getStringAt(deref);
        if (!str2.isEmpty()) {
            result += " → \"" + str2.left(64) + "\"";
        } else {
            QString sym2 = m_debugCore->getSymbolAt(deref);
            if (!sym2.isEmpty())
                result += " → <" + sym2 + ">";
        }
    }

    return result;
}

QString CPUInfoBox::getModuleAndOffset(uint64_t address) const
{
    auto regions = m_debugCore->getMemoryMap();
    for (const auto& region : regions) {
        if (address >= region.base && address < region.base + region.size) {
            QString modName = region.name;
            int lastSlash = modName.lastIndexOf('/');
            if (lastSlash >= 0)
                modName = modName.mid(lastSlash + 1);
            if (modName.isEmpty())
                continue;

            uint64_t rva = address - region.base;
            return QString("%1:$%2").arg(modName, QString::number(rva, 16).toUpper());
        }
    }
    return QString();
}

bool CPUInfoBox::isMemoryOperand(const QString& operand) const
{
    // Intel: [rsp+8], AT&T: (%rsp) or 0x8(%rbp)
    return operand.contains('[') || operand.contains('(');
}

QString CPUInfoBox::getSizePrefix(const QString& operand) const
{
    QString op = operand.trimmed().toLower();
    if (op.startsWith("byte ptr"))   return "byte ptr ";
    if (op.startsWith("word ptr"))   return "word ptr ";
    if (op.startsWith("dword ptr"))  return "dword ptr ";
    if (op.startsWith("qword ptr"))  return "qword ptr ";
    if (op.startsWith("xmmword ptr")) return "xmmword ptr ";
    if (op.startsWith("ymmword ptr")) return "ymmword ptr ";
    return "qword ptr "; // default for x64
}

uint64_t CPUInfoBox::readPointer(uint64_t address) const
{
    QByteArray data = m_debugCore->readMemory(address, 8);
    if (data.size() < 8) return 0;
    uint64_t val = 0;
    memcpy(&val, data.constData(), 8);
    return val;
}
