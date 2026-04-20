#include "SetWatchpointDialog.h"
#include "common/Configuration.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

SetWatchpointDialog::SetWatchpointDialog(uint64_t address, QWidget* parent)
    : QDialog(parent)
    , m_address(address)
{
    setWindowTitle("Set Watchpoint");
    setMinimumWidth(360);

    m_addressEdit = new QLineEdit(
        QString("0x%1").arg(address, 16, 16, QChar('0')).toUpper(), this);
    m_addressEdit->setReadOnly(true);
    m_addressEdit->setFont(ConfigFont("Disassembly"));

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("Write",        QVariant(false));
    m_typeCombo->addItem("Read / Write", QVariant(true));

    m_sizeCombo = new QComboBox(this);
    m_sizeCombo->addItem("1 byte",  QVariant(1));
    m_sizeCombo->addItem("2 bytes", QVariant(2));
    m_sizeCombo->addItem("4 bytes", QVariant(4));
    m_sizeCombo->addItem("8 bytes", QVariant(8));
    m_sizeCombo->setCurrentIndex(3);  // default 8 bytes

    auto* form = new QFormLayout;
    form->addRow("Address:", m_addressEdit);
    form->addRow("Type:",    m_typeCombo);
    form->addRow("Size:",    m_sizeCombo);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addWidget(buttons);
}

uint32_t SetWatchpointDialog::size() const
{
    return m_sizeCombo->currentData().toUInt();
}

bool SetWatchpointDialog::readWrite() const
{
    return m_typeCombo->currentData().toBool();
}
