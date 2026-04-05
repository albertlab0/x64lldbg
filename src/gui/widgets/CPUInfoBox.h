#pragma once

#include <QTableWidget>
#include "core/DebugCore.h"

class CPUInfoBox : public QTableWidget
{
    Q_OBJECT

public:
    explicit CPUInfoBox(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void updateInfo(uint64_t address);

private:
    void setupLayout();
    void applyStyle();
    void setupContextMenu();
    void setInfoLine(int row, const QString& text, const QColor& color = QColor());

    // Instruction analysis helpers
    bool isConditionalJump(const QString& mnemonic) const;
    bool isJumpTaken(const QString& mnemonic, uint64_t rflags) const;
    uint64_t resolveOperandValue(const QString& operand) const;
    QString formatAddress(uint64_t address) const;
    QString formatDerefChain(uint64_t address) const;
    QString getModuleAndOffset(uint64_t address) const;
    bool isMemoryOperand(const QString& operand) const;
    QString getSizePrefix(const QString& operand) const;
    uint64_t readPointer(uint64_t address) const;

    DebugCore* m_debugCore;
    QMap<QString, uint64_t> m_regCache; // cached register values for current update
};
