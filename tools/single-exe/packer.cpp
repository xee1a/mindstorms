// nxtpack <dist folder> <launcher.exe> <output.exe>
// Packs every file of the dist folder into the launcher, making one self-contained NXTStudio.exe.
#include "Pack.h"

#include <windows.h>
#include <compressapi.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<unsigned char> readAll(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

template <typename T> void put(std::vector<unsigned char> &out, T v)
{
    const auto *b = reinterpret_cast<const unsigned char *>(&v);
    out.insert(out.end(), b, b + sizeof v);
}

} // namespace

int wmain(int argc, wchar_t **argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "usage: nxtpack <dist folder> <launcher.exe> <output.exe>\n");
        return 2;
    }
    const fs::path dist = argv[1];
    std::vector<unsigned char> archive;
    int files = 0;
    for (const auto &e : fs::recursive_directory_iterator(dist)) {
        if (!e.is_regular_file()) continue;
        const std::u8string rel = fs::relative(e.path(), dist).generic_u8string();
        const std::vector<unsigned char> data = readAll(e.path());
        put<uint32_t>(archive, uint32_t(rel.size()));
        archive.insert(archive.end(), rel.begin(), rel.end());
        put<uint64_t>(archive, data.size());
        archive.insert(archive.end(), data.begin(), data.end());
        ++files;
    }

    COMPRESSOR_HANDLE c = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &c)) {
        std::fprintf(stderr, "CreateCompressor failed (%lu)\n", GetLastError());
        return 1;
    }
    SIZE_T needed = 0;
    Compress(c, archive.data(), archive.size(), nullptr, 0, &needed);
    std::vector<unsigned char> payload(needed);
    SIZE_T written = 0;
    if (!Compress(c, archive.data(), archive.size(), payload.data(), payload.size(), &written)) {
        std::fprintf(stderr, "Compress failed (%lu)\n", GetLastError());
        return 1;
    }
    CloseCompressor(c);
    payload.resize(written);

    pack::Trailer t{payload.size(), archive.size(), pack::fnv1a(payload.data(), payload.size()), {}};
    std::memcpy(t.magic, pack::Magic, sizeof t.magic);

    const std::vector<unsigned char> launcher = readAll(argv[2]);
    std::ofstream out(argv[3], std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(launcher.data()), std::streamsize(launcher.size()));
    out.write(reinterpret_cast<const char *>(payload.data()), std::streamsize(payload.size()));
    out.write(reinterpret_cast<const char *>(&t), sizeof t);
    if (!out) {
        std::fprintf(stderr, "couldn't write the output\n");
        return 1;
    }
    std::printf("packed %d files, %.1f MB -> %.1f MB\n", files, archive.size() / 1048576.0, (launcher.size() + payload.size()) / 1048576.0);
    return 0;
}
