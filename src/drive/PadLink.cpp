#include "PadLink.h"

#include <algorithm>
#include <cmath>

namespace drive {

PadAxes padAxes(const ControlInput &input)
{
    const auto pct = [](double v, int lo) { return std::clamp(int(std::lround(v * 100)), lo, 100); };
    return {pct(input.leftX, -100), pct(input.leftY, -100), pct(input.rightX, -100),
            pct(input.rightY, -100), pct(input.leftTrigger, 0), pct(input.rightTrigger, 0)};
}

QByteArray encodePadMessage(const PadAxes &axes, uint16_t buttons)
{
    QByteArray m;
    m.append('P');
    for (int v : axes) m.append(char(std::clamp(v, -100, 100) + 101));
    for (int i = 0; i < 4; ++i) m.append(char(0x40 | ((buttons >> (4 * i)) & 0x0f)));
    return m;
}

} // namespace drive
