#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>

/**
 * LEGO's own files on the brick (system sounds, Try Me programs) and every sound file. The app keeps a copy of
 * each one it reads from a brick, so a factory reset can put back files that were deleted, even on another brick.
 * For the firmware's three system sounds it also has its own synthesised stand-ins.
 */
namespace nxt::systemfiles {

/** Folder on the PC that holds the copies. */
QString backupDir();
/** Whether a reset keeps this file and puts it back when it is missing (sounds and Try Me programs). */
bool restorable(const QString &name);
bool isKept(const QString &name);
/** Saves a copy of a file read from a brick, unless one is already kept. */
void keep(const QString &name, const QByteArray &data);
/** Files a reset can put back: the kept copies plus the app's own system sounds. */
QStringList available();
/** Contents to write for `name`: the kept copy, else the app's own sound, else empty. */
QByteArray contents(const QString &name);
/** The app's stand-in for "! Startup.rso", "! Click.rso" or "! Attention.rso" (NXT .rso format, as on the brick); empty otherwise. */
QByteArray ownSound(const QString &name);

} // namespace nxt::systemfiles
