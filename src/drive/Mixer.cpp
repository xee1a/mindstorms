#include "Mixer.h"

#include <algorithm>
#include <cmath>

namespace drive {

namespace {
double clamp1(double v) { return std::clamp(v, -1.0, 1.0); }
} // namespace

std::pair<double, double> applyDeadzone(double x, double y, double dz)
{
    const double mag = std::hypot(x, y);
    if (mag <= dz || mag == 0) return {0, 0};
    const double scaled = std::min(1.0, (mag - dz) / (1 - dz));
    return {x / mag * scaled, y / mag * scaled};
}

double applyExpo(double v, double expo) { return (1 - expo) * v + expo * v * v * v; }

std::array<int, 3> mix(const ControlInput &input, const DriveConfig &cfg)
{
    const auto [lx, ly] = applyDeadzone(input.leftX, input.leftY, cfg.deadzone);
    const auto [rx, ry] = applyDeadzone(input.rightX, input.rightY, cfg.deadzone);
    const auto shape = [&](double v) { return applyExpo(clamp1(v), cfg.expo); };

    double left = 0;
    double right = 0;
    if (cfg.mode == DriveMode::Tank) {
        left = shape(ly);
        right = shape(ry);
    } else {
        const bool racing = cfg.mode == DriveMode::Racing;
        const double throttle = racing ? shape(input.rightTrigger - input.leftTrigger) : shape(ly);
        const double steerAxis = racing ? lx : (std::abs(rx) > std::abs(lx) ? rx : lx);
        const double turn = shape(steerAxis);
        left = throttle + turn;
        right = throttle - turn;
        // Keep the ratio between sides when one exceeds full scale.
        const double over = std::max({1.0, std::abs(left), std::abs(right)});
        left /= over;
        right /= over;
    }

    std::array<double, 3> out{};
    const double scale = std::clamp(cfg.maxPower, 0, 100);
    out[cfg.leftMotor] += (cfg.invertLeft ? -left : left) * scale;
    if (cfg.rightMotor != cfg.leftMotor) out[cfg.rightMotor] += (cfg.invertRight ? -right : right) * scale;

    if (cfg.auxMotor && *cfg.auxMotor != cfg.leftMotor && *cfg.auxMotor != cfg.rightMotor) {
        const int dir = (input.rightBumper ? 1 : 0) - (input.leftBumper ? 1 : 0);
        out[*cfg.auxMotor] = dir * (cfg.invertAux ? -1 : 1) * std::clamp(cfg.auxPower, 0, 100);
    }

    std::array<int, 3> result{};
    for (int i = 0; i < 3; ++i) result[i] = static_cast<int>(std::lround(out[i])) + 0; // + 0 normalises -0
    return result;
}

} // namespace drive
