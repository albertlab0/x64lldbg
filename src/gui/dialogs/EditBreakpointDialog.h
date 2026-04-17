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

private:
    void setupUI(const BreakpointInfo& bp);

    DebugCore* m_debugCore;
    QLineEdit* m_editBreakCondition;
    QLineEdit* m_editLogText;
    QLineEdit* m_editLogCondition;
    QCheckBox* m_checkFastResume;
};
