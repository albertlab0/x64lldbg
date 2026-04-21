#pragma once

#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QVector>
#include "core/DebugCore.h"

class CPUStack;

// Delegate that paints the per-row frame bracket on top of the value
// column, x64dbg-style. The bitfield per row (0=none, 1=top, 2=bottom,
// 4=middle) and the function range it belongs to live on the delegate
// so refresh() only has to update them once.
class StackFrameBracketDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit StackFrameBracketDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void setFrameBitfield(QVector<int> bits) { m_bits = std::move(bits); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    QVector<int> m_bits;  // per-row frame membership bitfield
};

class CPUStack : public QTableWidget
{
    Q_OBJECT

public:
    explicit CPUStack(DebugCore* debugCore, QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void setupColumns();
    void applyStyle();

    DebugCore* m_debugCore;
    StackFrameBracketDelegate* m_bracketDelegate;
};
