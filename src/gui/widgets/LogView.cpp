#include "LogView.h"
#include "common/Configuration.h"

LogView::LogView(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setFont(ConfigFont("Log"));

    QColor bg = ConfigColor("DisassemblyBackgroundColor");
    QColor fg = ConfigColor("DisassemblyTextColor");

    setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; }"
    ).arg(bg.name(), fg.name()));

    // Initial log messages
    appendPlainText("[x64lldbg] Debugger initialized");
    appendPlainText("[x64lldbg] Ready");
}

void LogView::addMessage(const QString& message)
{
    appendPlainText(message);
}
