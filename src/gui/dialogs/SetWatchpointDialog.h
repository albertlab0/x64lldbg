#pragma once

#include <QDialog>
#include <cstdint>

class QComboBox;
class QLineEdit;
class QPushButton;

class SetWatchpointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SetWatchpointDialog(uint64_t address, QWidget* parent = nullptr);

    uint64_t address() const { return m_address; }
    uint32_t size() const;
    bool readWrite() const;  // true = R/W, false = write-only

private:
    uint64_t m_address;
    QLineEdit* m_addressEdit;
    QComboBox* m_typeCombo;
    QComboBox* m_sizeCombo;
};
