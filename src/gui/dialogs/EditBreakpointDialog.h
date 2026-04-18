#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include "core/DebugCore.h"

class EditBreakpointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditBreakpointDialog(DebugCore* debugCore, const BreakpointInfo& bp,
                                   QWidget* parent = nullptr);

    QString breakCondition() const;
    QString logText() const;
    QString logCondition() const;
    bool fastResume() const;
    QString dumpAddress() const;
    QString dumpSize() const;
    QString dumpFilename() const;

private:
    void setupUI(const BreakpointInfo& bp);

    DebugCore* m_debugCore;
    QLineEdit* m_editBreakCondition;
    QLineEdit* m_editLogText;
    QLineEdit* m_editLogCondition;
    QCheckBox* m_checkFastResume;
    QLineEdit* m_editDumpAddress;
    QLineEdit* m_editDumpSize;
    QLineEdit* m_editDumpFilename;
};
