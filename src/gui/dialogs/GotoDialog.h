#pragma once

#include <QDialog>
#include <cstdint>

class QLineEdit;
class QLabel;
class QPushButton;
class DebugCore;

class GotoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GotoDialog(DebugCore* debugCore, QWidget* parent = nullptr);

    uint64_t resultAddress() const { return m_resultAddress; }

private slots:
    void onTextChanged(const QString& text);
    void onOkClicked();

private:
    void applyStyle();
    uint64_t evaluateExpression(const QString& expr, bool* ok);
    uint64_t resolveSymbol(const QString& name, bool* ok);
    uint64_t resolveModuleBase(const QString& moduleName, bool* ok);

    DebugCore* m_debugCore;
    QLineEdit* m_lineEdit;
    QLabel* m_statusLabel;
    QPushButton* m_okButton;
    uint64_t m_resultAddress = 0;
};
