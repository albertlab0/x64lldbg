#pragma once

#include <QDialog>
#include <QPlainTextEdit>

class DebugCore;
class QPushButton;
class QLabel;

class ScriptEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScriptEditorDialog(DebugCore* debugCore, QWidget* parent = nullptr);

signals:
    void outputProduced(const QString& text);
    void errorProduced(const QString& text);

private slots:
    void runScript();
    void loadScript();
    void saveScript();

private:
    void applyStyle();

    DebugCore* m_debugCore;

    QPlainTextEdit* m_editor;
    QPlainTextEdit* m_outputPane;
    QLabel* m_statusLabel;
};
