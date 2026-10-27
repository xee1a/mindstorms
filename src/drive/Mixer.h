#pragma once
// Turns gamepad sticks/triggers into motor powers. Pure functions, unit tested.

#include <array>
#include <optional>
#include <utility>

namespace drive {

enum class DriveMode { Arcade, Tank, Racing };

struct DriveConfig {
    DriveMode mode = DriveMode::Arcade;
    int leftMotor = 1;  // 0..2 = A..C
    int rightMotor = 2;
    /** Third motor (e.g. an arm), driven by the shoulder buttons. */
    std::optional<int> auxMotor = 0;
    bool invertLeft = false;
    bool invertRight = false;
    bool invertAux = false;
    int maxPower = 100;    // 10..100
    double deadzone = 0.12; // 0..0.5
    double expo = 0.35;    // 0 linear .. 1 strongly expo
    int auxPower = 60;
};

/** Normalised controller input. Stick Y is up-positive. */
struct ControlInput {
    double leftX = 0, leftY = 0;
    double rightX = 0, rightY = 0;
    double leftTrigger = 0, rightTrigger = 0; // 0..1
    bool leftBumper = false, rightBumper = false;
};

/** Scaled radial deadzone: inside `dz` becomes 0, the rest is rescaled to keep full range. */
std::pair<double, double> applyDeadzone(double x, double y, double dz);
/** Blend between linear and cubic response for finer control near centre. */
double applyExpo(double v, double expo);
/** Motor power for outputs A, B, C (-100..100). */
std::array<int, 3> mix(const ControlInput &input, const DriveConfig &cfg);

} // namespace drive
