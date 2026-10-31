#pragma once

#include <QJsonObject>
#include <QString>

namespace compile {

struct NxcResult {
    /** NXC source for nbc, empty when `error` is set. */
    QString source;
    /** A problem the user can fix in their blocks (already translated). */
    QString error;
};

/**
 * Turns a workspace (Workspace::toJson) into an NXC program for the standard 1.28+ firmware.
 * Every stack becomes a task; blocks map onto the NXC API, with small helpers for the
 * parts it lacks on that firmware (floor, formatted numbers, sync pairs chosen at run time).
 */
NxcResult generateNxc(const QJsonObject &workspace, const QString &programName);

/** Turns any text into what the brick's font can show: Polish letters lose their marks, the rest becomes '?'. */
QString toNxtText(const QString &text);

} // namespace compile
