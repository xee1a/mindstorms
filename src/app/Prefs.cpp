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
bool Prefs::beepOnConnect() const { return flag("brick/beepOnConnect", true); }
void Prefs::setBeepOnConnect(bool on) { setFlag("brick/beepOnConnect", on); }

int Prefs::startTab() const { return std::clamp(QSettings().value(QStringLiteral("window/startTab"), -1).toInt(), -1, 2); }

void Prefs::setStartTab(int tab)
{
    QSettings().setValue(QStringLiteral("window/startTab"), tab);
    emit changed();
}

drive::DriveConfig Prefs::driveConfig() const
{
    drive::DriveConfig cfg;
    QSettings s;
    s.beginGroup(QStringLiteral("drive"));
    cfg.mode = drive::DriveMode(std::clamp(s.value("mode", int(cfg.mode)).toInt(), 0, 2));
    cfg.leftMotor = std::clamp(s.value("leftMotor", cfg.leftMotor).toInt(), 0, 2);
    cfg.rightMotor = std::clamp(s.value("rightMotor", cfg.rightMotor).toInt(), 0, 2);
    const int aux = s.value("auxMotor", cfg.auxMotor.value_or(-1)).toInt();
    cfg.auxMotor = aux < 0 || aux > 2 ? std::nullopt : std::optional<int>(aux);
    cfg.invertLeft = s.value("invertLeft", false).toBool();
    cfg.invertRight = s.value("invertRight", false).toBool();
    cfg.invertAux = s.value("invertAux", false).toBool();
    cfg.maxPower = std::clamp(s.value("maxPower", cfg.maxPower).toInt(), 10, 100);
    cfg.auxPower = std::clamp(s.value("auxPower", cfg.auxPower).toInt(), 10, 100);
    cfg.deadzone = s.value("deadzone", cfg.deadzone).toDouble();
    cfg.expo = s.value("expo", cfg.expo).toDouble();
    return cfg;
}

void Prefs::setDriveConfig(const drive::DriveConfig &cfg)
{
    QSettings s;
    s.beginGroup(QStringLiteral("drive"));
    s.setValue("mode", int(cfg.mode));
    s.setValue("leftMotor", cfg.leftMotor);
    s.setValue("rightMotor", cfg.rightMotor);
    s.setValue("auxMotor", cfg.auxMotor.value_or(-1));
    s.setValue("invertLeft", cfg.invertLeft);
    s.setValue("invertRight", cfg.invertRight);
    s.setValue("invertAux", cfg.invertAux);
    s.setValue("maxPower", cfg.maxPower);
    s.setValue("auxPower", cfg.auxPower);
    s.setValue("deadzone", cfg.deadzone);
    s.setValue("expo", cfg.expo);
    emit driveConfigChanged(cfg);
}

void Prefs::resetAll()
{
    QSettings().clear();
    emit changed();
    emit driveConfigChanged(driveConfig());
}
