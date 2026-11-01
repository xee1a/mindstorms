#include "Prefs.h"

#include <QSettings>
#include <algorithm>

Prefs &Prefs::instance()
{
    static Prefs p;
    return p;
}

bool Prefs::flag(const char *key, bool fallback) const { return QSettings().value(QLatin1String(key), fallback).toBool(); }

void Prefs::setFlag(const char *key, bool on)
{
    QSettings().setValue(QLatin1String(key), on);
    emit changed();
}

bool Prefs::showGrid() const { return flag("editor/grid", true); }
void Prefs::setShowGrid(bool on) { setFlag("editor/grid", on); }
bool Prefs::autosave() const { return flag("editor/autosave", true); }
void Prefs::setAutosave(bool on) { setFlag("editor/autosave", on); }
bool Prefs::autoConnect() const { return flag("brick/autoConnect", true); }
void Prefs::setAutoConnect(bool on) { setFlag("brick/autoConnect", on); }
bool Prefs::editorSounds() const { return flag("editor/sounds", true); }
void Prefs::setEditorSounds(bool on) { setFlag("editor/sounds", on); }
bool Prefs::beepOnConnect() const { return flag("brick/beepOnConnect", true); }
void Prefs::setBeepOnConnect(bool on) { setFlag("brick/beepOnConnect", on); }

int Prefs::startTab() const { return std::clamp(QSettings().value(QStringLiteral("window/startTab"), -1).toInt(), -1, 2); }

void Prefs::setStartTab(int tab)
{
    QSettings().setValue(QStringLiteral("window/startTab"), tab);
    emit changed();
}

int Prefs::padLayout() const { return std::clamp(QSettings().value(QStringLiteral("controller/layout"), 0).toInt(), 0, 2); }

void Prefs::setPadLayout(int layout)
{
    QSettings().setValue(QStringLiteral("controller/layout"), layout);
    emit changed();
}

void Prefs::resetAll()
{
    QSettings().clear();
    emit changed();
}
