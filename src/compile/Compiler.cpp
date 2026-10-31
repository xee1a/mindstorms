#include "Compiler.h"
#include "NxcGenerator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

namespace compile {

namespace {

/** Translation context for this file's messages (lupdate reads Text::tr calls). */
struct Text {
    Q_DECLARE_TR_FUNCTIONS(Compiler)
};

constexpr int CompileTimeoutMs = 30'000;

} // namespace

QString nbcPath()
{
    const QDir app(QCoreApplication::applicationDirPath());
    for (const QString &candidate : {app.filePath(QStringLiteral("tools/nbc.exe")), app.filePath(QStringLiteral("nbc.exe"))})
        if (QFileInfo::exists(candidate)) return candidate;
    return {};
}

CompileResult compileWorkspace(const QJsonObject &workspace, const QString &programName)
{
    CompileResult result;
    const NxcResult nxc = generateNxc(workspace, programName);
    if (!nxc.error.isEmpty()) {
        result.error = nxc.error;
        return result;
    }
    result.source = nxc.source;

    const QString nbc = nbcPath();
    if (nbc.isEmpty()) {
        result.error = Text::tr("The NXC compiler (tools\\nbc.exe) is missing from the NXT Studio folder. Reinstall the app.");
        return result;
    }
    QTemporaryDir dir;
    if (!dir.isValid()) {
        result.error = Text::tr("Couldn't create a temporary folder for compiling.");
        return result;
    }
    const QString src = dir.filePath(QStringLiteral("program.nxc"));
    const QString out = dir.filePath(QStringLiteral("program.rxe"));
    QFile f(src);
    if (!f.open(QIODevice::WriteOnly) || f.write(nxc.source.toLatin1()) < 0) {
        result.error = Text::tr("Couldn't write the program for the compiler.");
        return result;
    }
    f.close();

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setWorkingDirectory(dir.path());
    p.start(nbc, {QStringLiteral("-O=%1").arg(QDir::toNativeSeparators(out)), QDir::toNativeSeparators(src)});
    if (!p.waitForStarted(5000)) {
        result.error = Text::tr("Couldn't start the NXC compiler.");
        return result;
    }
    if (!p.waitForFinished(CompileTimeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        result.error = Text::tr("The NXC compiler took too long and was stopped.");
        return result;
    }
    // nbc prints "# Status:" progress lines; keep the rest, which are the errors.
    QStringList lines;
    for (const QString &line : QString::fromLocal8Bit(p.readAll()).split(QLatin1Char('\n')))
        if (!line.trimmed().isEmpty() && !line.startsWith(QLatin1String("# Status"))) lines << line.trimmed();
    result.log = lines.join(QLatin1Char('\n'));

    QFile rxe(out);
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0 || !rxe.open(QIODevice::ReadOnly)) {
        // Generated code should always compile, so this is a bug in NXT Studio rather than in the blocks.
        result.error = Text::tr("The program couldn't be compiled. This is a bug in NXT Studio; the details show the compiler's message.");
        return result;
    }
    result.rxe = rxe.readAll();
    return result;
}

} // namespace compile
