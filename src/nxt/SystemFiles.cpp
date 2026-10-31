#include "SystemFiles.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace nxt::systemfiles {

namespace {

constexpr int SampleRate = 8000;
const QString Startup = QStringLiteral("! Startup.rso");
const QString Click = QStringLiteral("! Click.rso");
const QString Attention = QStringLiteral("! Attention.rso");

bool safeName(const QString &name)
{
    static const QString bad = QStringLiteral("/\\:*?\"<>|");
    return !name.isEmpty() && name.size() <= 19 && std::none_of(name.begin(), name.end(), [](QChar c) { return bad.contains(c); });
}

/** Adds a tone with a short attack and an exponential decay; `decay` is the time constant in seconds. */
void tone(std::vector<float> &out, double startS, double lengthS, double hz, double level, double decay)
{
    const int from = int(startS * SampleRate);
    const int count = int(lengthS * SampleRate);
    if (out.size() < size_t(from + count)) out.resize(size_t(from + count), 0.f);
    for (int i = 0; i < count; ++i) {
        const double t = double(i) / SampleRate;
        const double env = std::min(1.0, t / 0.004) * std::exp(-t / decay) * std::min(1.0, (lengthS - t) / 0.006);
        const double w = 2 * std::numbers::pi * hz * t;
        out[size_t(from + i)] += float(level * env * (std::sin(w) + 0.25 * std::sin(2 * w)));
    }
}

/** NXT sampled sound: format 0x0101, big-endian sample count and rate, play mode 0, then unsigned 8-bit samples. */
QByteArray rso(const std::vector<float> &samples)
{
    QByteArray out;
    const auto n = uint16_t(samples.size());
    out.append(char(0x01)).append(char(0x01));
    out.append(char(n >> 8)).append(char(n & 0xff));
    out.append(char(SampleRate >> 8)).append(char(SampleRate & 0xff));
    out.append(char(0)).append(char(0));
    for (float s : samples) out.append(char(std::clamp(int(std::lround(128 + 127 * s)), 1, 255)));
    return out;
}

} // namespace

QString backupDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/brick-files");
}

bool restorable(const QString &name)
{
    if (name.endsWith(QLatin1String(".sys"), Qt::CaseInsensitive)) return false; // never deleted, so never restored
    return name.startsWith(QLatin1String("! ")) || name.startsWith(QLatin1String("Try-"), Qt::CaseInsensitive) ||
           name.endsWith(QLatin1String(".rso"), Qt::CaseInsensitive);
}

bool isKept(const QString &name) { return safeName(name) && QFile::exists(backupDir() + '/' + name); }

void keep(const QString &name, const QByteArray &data)
{
    if (!restorable(name) || !safeName(name) || data.isEmpty() || isKept(name)) return;
    QDir().mkpath(backupDir());
    QSaveFile f(backupDir() + '/' + name);
    if (f.open(QIODevice::WriteOnly) && f.write(data) == data.size()) f.commit();
}

QStringList available()
{
    QStringList names = {Startup, Click, Attention};
    for (const QString &n : QDir(backupDir()).entryList(QDir::Files))
        if (restorable(n) && !names.contains(n, Qt::CaseInsensitive)) names << n;
    return names;
}

QByteArray contents(const QString &name)
{
    if (isKept(name)) {
        QFile f(backupDir() + '/' + name);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray data = f.readAll();
            if (!data.isEmpty()) return data;
        }
    }
    return ownSound(name);
}

QByteArray ownSound(const QString &name)
{
    std::vector<float> s;
    if (name.compare(Startup, Qt::CaseInsensitive) == 0) {
        // A rising three-note chime, about half a second.
        tone(s, 0.00, 0.13, 1046.5, 0.45, 0.10);
        tone(s, 0.11, 0.13, 1318.5, 0.45, 0.10);
        tone(s, 0.22, 0.30, 1568.0, 0.50, 0.12);
    } else if (name.compare(Click, Qt::CaseInsensitive) == 0) {
        tone(s, 0.0, 0.025, 2200.0, 0.6, 0.006);
    } else if (name.compare(Attention, Qt::CaseInsensitive) == 0) {
        tone(s, 0.00, 0.05, 1480.0, 0.5, 0.05);
        tone(s, 0.07, 0.05, 1480.0, 0.5, 0.05);
    } else {
        return {};
    }
    return rso(s);
}

} // namespace nxt::systemfiles
