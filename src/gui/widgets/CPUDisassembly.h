#pragma once

#include <QTableWidget>
#include "core/DebugCore.h"

class CPUDisassembly : public QTableWidget
{
    Q_OBJECT

public:
    explicit CPUDisassembly(DebugCore* debugCore, QWidget* parent = nullptr);

    uint64_t selectedAddress() const;
    const QVector<DisassemblyLine>& getLines() const { return m_lines; }

public slots:
    void refresh();
    void goToAddress(uint64_t address);

signals:
    void addressSelected(uint64_t address);
    void linesChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void setupColumns();
    void applyStyle();
    void setupContextMenu();
    void populateFromAddress(uint64_t address);
    void rebuildTable();
    void loadMoreBelow();
    void updateHighlights(uint64_t pc);
    void promptSetLabel();
    QColor colorForMnemonic(const QString& mnemonic) const;
    QColor bgColorForMnemonic(const QString& mnemonic) const;

    DebugCore* m_debugCore;
    uint64_t m_baseAddress = 0;
    uint64_t m_gotoAddress = 0;  // user navigation highlight (Ctrl+G)
    QVector<DisassemblyLine> m_lines;
};
