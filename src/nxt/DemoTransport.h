#pragma once

#include "Transport.h"

#include <QElapsedTimer>
#include <array>
#include <deque>

namespace nxt {

/** A simulated brick that answers direct commands, for trying the app without hardware. */
class DemoTransport : public Transport {
public:
    DemoTransport();
    bool isDemo() const override { return true; }
    QString label() const override { return QStringLiteral("Demo NXT"); }
    void write(const QByteArray &packet) override;
    QByteArray read(int timeoutMs) override;

private:
    struct Motor {
        int power = 0;
        bool running = false;
        double tacho = 0;
    };

    void tick();
    QByteArray handle(const QByteArray &p);
    int simulateSensor(int type, double t, int port) const;

    std::deque<QByteArray> m_replies;
    std::array<Motor, 3> m_motors{};
    std::array<int, 4> m_sensorTypes{};
    QElapsedTimer m_clock;
    qint64 m_lastTickMs = 0;
};

} // namespace nxt
