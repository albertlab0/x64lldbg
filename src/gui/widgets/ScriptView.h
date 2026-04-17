#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QStringList>

class DebugCore;
class QPushButton;

class ScriptView : public QWidget
{
    Q_OBJECT

public:
    explicit ScriptView(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void appendOutput(const QString& text);
    void appendError(const QString& text);

private slots:
    void executeInput();
    void historyUp();
    void historyDown();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void applyStyle();
    void setupContextMenu();

    DebugCore* m_debugCore;

    QPlainTextEdit* m_output;
    QLineEdit* m_input;
    QPushButton* m_editorBtn;

    // Command history
    QStringList m_history;
    int m_historyIndex = -1;
    static constexpr int MAX_HISTORY = 200;
};
