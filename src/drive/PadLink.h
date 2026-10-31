#pragma once
// The gamepad as seen by block programs: the same numbers whether the program runs
// from the PC or on the brick, where the app streams them into an inbox. Unit tested.

#include "Mixer.h"

#include <QByteArray>
#include <array>
#include <cstdint>

namespace drive {

/** Axes block programs read: sticks -100..100 (up and right positive), triggers 0..100. */
enum class PadAxis { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger, Count };
using PadAxes = std::array<int, int(PadAxis::Count)>;

PadAxes padAxes(const ControlInput &input);

/**
 * Inbox message for the brick: 'P', then one byte per axis (value + 101, so never 0),
 * then the 16 button bits as four bytes of 0x40 | nibble (lowest bits first).
 * Bit n is button n in drive::PadButton order. Decoded by the NXC the generator writes.
 */
QByteArray encodePadMessage(const PadAxes &axes, uint16_t buttons);

} // namespace drive
