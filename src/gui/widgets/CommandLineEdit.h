#pragma once

#include <QLineEdit>
#include <QStringList>

class DebugCore;
class CPUDump;
class LogView;

class CommandLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit CommandLineEdit(DebugCore* debugCore, QWidget* parent = nullptr);

    void setDumpWidget(CPUDump* dump) { m_dump = dump; }
    void setLogView(LogView* log) { m_logView = log; }

signals:
    void commandOutput(const QString& text);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void executeCommand(const QString& cmd);
    uint64_t evaluateExpression(const QString& expr, bool* ok);

    DebugCore* m_debugCore;
    CPUDump* m_dump = nullptr;
    LogView* m_logView = nullptr;
    QStringList m_history;
    int m_historyIndex = -1;
};
