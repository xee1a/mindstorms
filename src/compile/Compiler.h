#pragma once

#include <QByteArray>
#include <QString>

class QJsonObject;

namespace compile {

struct CompileResult {
    /** The .rxe program, empty on failure. */
    QByteArray rxe;
    /** Short reason for the user, empty on success. */
    QString error;
    /** The compiler's own output, for the details pane. */
    QString log;
    /** The generated NXC, for the details pane. */
    QString source;
};

/** Where the bundled nbc.exe lives; empty when it's missing. */
QString nbcPath();

/** Compiles NXC source with nbc. Blocks for a second or two. */
CompileResult compileSource(const QString &source);

/** Generates NXC from the workspace and compiles it with nbc. Blocks for a second or two. */
CompileResult compileWorkspace(const QJsonObject &workspace, const QString &programName);

} // namespace compile
