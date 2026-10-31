#pragma once

#include <QObject>
#include <QString>

/**
 * User preferences that take effect immediately, stored with QSettings.
 * Theme, accent and language are read once at startup instead (see theme::init and main.cpp).
 */
class Prefs : public QObject {
    Q_OBJECT
public:
    static Prefs &instance();

    bool showGrid() const;
    void setShowGrid(bool on);
    bool autosave() const;
    void setAutosave(bool on);
    bool autoConnect() const;
    void setAutoConnect(bool on);
    bool editorSounds() const;
    void setEditorSounds(bool on);
    bool beepOnConnect() const;
    void setBeepOnConnect(bool on);
    /** Tab shown at startup: 0..2, or -1 for the one open last time. */
    int startTab() const;
    void setStartTab(int tab);

    /** Clears every saved setting (window layout, theme, language, sensors). */
    void resetAll();

signals:
    void changed();

private:
    Prefs() = default;
    bool flag(const char *key, bool fallback) const;
    void setFlag(const char *key, bool on);
};
