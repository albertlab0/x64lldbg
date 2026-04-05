#pragma once

#include <QObject>
#include <QColor>
#include <QFont>
#include <QKeySequence>
#include <QMap>
#include <QString>

struct Shortcut {
    QString name;
    QKeySequence hotkey;
};

class Configuration : public QObject
{
    Q_OBJECT

public:
    enum Theme {
        X64dbgDefault,
        ModernLight,
        CutterDark
    };
    Q_ENUM(Theme)

    static Configuration* instance();

    // Theme
    Theme currentTheme() const { return m_theme; }
    void setTheme(Theme theme);
    bool isDarkTheme() const { return m_theme == CutterDark; }
    bool isLightTheme() const { return m_theme == X64dbgDefault || m_theme == ModernLight; }

    // Colors
    QColor getColor(const QString& id) const;
    void setColor(const QString& id, const QColor& color);

    // Fonts
    QFont getFont(const QString& id) const;

    // Shortcuts
    Shortcut getShortcut(const QString& id) const;
    const QMap<QString, Shortcut>& getShortcuts() const { return m_shortcuts; }

signals:
    void colorsUpdated();
    void fontsUpdated();
    void shortcutsUpdated();
    void themeChanged();

private:
    Configuration();
    void loadDefaults();
    void loadX64dbgDefaultColors();
    void loadModernLightColors();
    void loadCutterDarkColors();
    void loadDefaultShortcuts();
    void loadDefaultFonts();

    Theme m_theme = X64dbgDefault;
    QMap<QString, QColor> m_colors;
    QMap<QString, Shortcut> m_shortcuts;
    QMap<QString, QFont> m_fonts;

    static Configuration* s_instance;
};

// Convenience macros
#define Config() (Configuration::instance())
#define ConfigColor(x) (Config()->getColor(x))
#define ConfigFont(x) (Config()->getFont(x))
