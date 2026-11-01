#pragma once
// Layout of the single-file NXTStudio.exe: the launcher, then the packed app, then a trailer.
//
//   [launcher exe][payload: LZMS-compressed archive][Trailer]
//
// The archive is a list of files: u32 path length, UTF-8 path with '/', u64 size, the bytes.
// Plain Win32 only (no Qt), so the launcher runs before any Qt DLL exists on the PC.

#include <cstdint>

namespace pack {

constexpr char Magic[8] = {'N', 'X', 'T', 'P', 'A', 'C', 'K', '1'};

struct Trailer {
    uint64_t compressedSize;
    uint64_t rawSize;
    /** FNV-1a of the payload: names the folder the app is unpacked into, so each build gets its own. */
    uint64_t hash;
    char magic[8];
};
static_assert(sizeof(Trailer) == 32);

inline uint64_t fnv1a(const unsigned char *data, uint64_t size)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint64_t i = 0; i < size; ++i) h = (h ^ data[i]) * 0x100000001b3ull;
    return h;
}

} // namespace pack
