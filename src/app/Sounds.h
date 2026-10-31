#pragma once

namespace sfx {

enum class Sound {
    Pick,   // a block is picked up
    Snap,   // it clicks into place
    Drop,   // it's put down on its own
    Delete, // it's thrown away
};

/** Plays a short editor sound (made up at startup, no files), unless sounds are off in Settings. */
void play(Sound sound);

} // namespace sfx
