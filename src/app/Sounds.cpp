#include "Sounds.h"
#include "Prefs.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

namespace sfx {

namespace {

constexpr int Rate = 22050;
constexpr double Pi = 3.14159265358979323846;

/** A mono 16-bit WAV of `ms` milliseconds; `sample(t)` gives -1..1 for t in seconds. */
QByteArray wav(int ms, const std::function<double(double)> &sample)
{
    const int n = Rate * ms / 1000;
    QByteArray data(n * 2, Qt::Uninitialized);
    auto *out = reinterpret_cast<int16_t *>(data.data());
    for (int i = 0; i < n; ++i) out[i] = int16_t(std::clamp(sample(double(i) / Rate), -1.0, 1.0) * 32767 * 0.35);

    QByteArray w;
    const auto u32 = [&](uint32_t v) { w.append(reinterpret_cast<const char *>(&v), 4); };
    const auto u16 = [&](uint16_t v) { w.append(reinterpret_cast<const char *>(&v), 2); };
    w.append("RIFF");
    u32(uint32_t(36 + data.size()));
    w.append("WAVEfmt ");
    u32(16);
    u16(1); // PCM
    u16(1); // mono
    u32(Rate);
    u32(Rate * 2);
    u16(2);
    u16(16);
    w.append("data");
    u32(uint32_t(data.size()));
    w.append(data);
    return w;
}

/** Sine with a fast attack and exponential decay. */
double pluck(double t, double hz, double decay) { return std::sin(2 * Pi * hz * t) * std::exp(-t * decay) * std::min(1.0, t * 800); }

const std::array<QByteArray, 4> &sounds()
{
    static const std::array<QByteArray, 4> s{
        // Pick: a light, high tick.
        wav(45, [](double t) { return 0.7 * pluck(t, 1650, 90); }),
        // Snap: two quick clicks, like a brick clicking onto studs.
        wav(90, [](double t) { return 0.8 * pluck(t, 1250, 70) + (t > 0.032 ? 0.9 * pluck(t - 0.032, 1900, 80) : 0.0); }),
        // Drop: a soft low knock.
        wav(70, [](double t) { return 0.9 * pluck(t, 420, 60) + 0.3 * pluck(t, 840, 90); }),
        // Delete: a falling whoosh.
        wav(170, [](double t) {
            const double hz = 700 - 2600 * t;
            const double phase = 2 * Pi * (700 * t - 1300 * t * t);
            return 0.6 * std::sin(phase) * std::exp(-t * 14) * std::min(1.0, t * 300) * (hz > 0 ? 1 : 0);
        }),
    };
    return s;
}

} // namespace

void play(Sound sound)
{
    if (!Prefs::instance().editorSounds()) return;
    // Drags can fire pick and drop within a few milliseconds; don't let a tick cut off a snap.
    static QElapsedTimer last;
    static Sound lastSound = Sound::Pick;
    if (last.isValid() && last.elapsed() < 60 && lastSound != Sound::Pick && sound == Sound::Pick) return;
    last.start();
    lastSound = sound;
    const QByteArray &w = sounds()[size_t(sound)];
    PlaySoundW(reinterpret_cast<LPCWSTR>(w.constData()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

} // namespace sfx
