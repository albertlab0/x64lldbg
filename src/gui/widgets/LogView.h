#pragma once

#include <QPlainTextEdit>

class LogView : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LogView(QWidget* parent = nullptr);

public slots:
    void addMessage(const QString& message);
};
