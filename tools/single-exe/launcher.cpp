// The single-file NXTStudio.exe: unpacks the app packed onto its end into
// %LOCALAPPDATA%\NXT Studio\app\<build>, once per build, then starts it from there.
#include "Pack.h"

#include <windows.h>
#include <compressapi.h>
#include <shlobj.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void fail(const std::wstring &text)
{
    MessageBoxW(nullptr, text.c_str(), L"NXT Studio", MB_ICONERROR | MB_OK);
}

fs::path selfPath()
{
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), DWORD(buf.size()));
        if (n < buf.size()) return fs::path(buf.substr(0, n));
        buf.resize(buf.size() * 2);
    }
}

/** Unpacks the archive into `dir`; false on any error. */
bool unpack(const std::vector<unsigned char> &archive, const fs::path &dir)
{
    size_t at = 0;
    const auto take = [&](void *dst, size_t n) {
        if (at + n > archive.size()) return false;
        std::memcpy(dst, archive.data() + at, n);
        at += n;
        return true;
    };
    while (at < archive.size()) {
        uint32_t nameLen = 0;
        uint64_t size = 0;
        if (!take(&nameLen, sizeof nameLen) || at + nameLen > archive.size()) return false;
        const std::u8string name(reinterpret_cast<const char8_t *>(archive.data() + at), nameLen);
        at += nameLen;
        if (!take(&size, sizeof size) || at + size > archive.size()) return false;
        const fs::path target = dir / fs::path(name);
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(archive.data() + at), std::streamsize(size));
        if (!out) return false;
        at += size;
    }
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const fs::path self = selfPath();
    std::ifstream in(self, std::ios::binary);
    in.seekg(0, std::ios::end);
    const auto fileSize = uint64_t(in.tellg());
    pack::Trailer t{};
    if (fileSize < sizeof t) return fail(L"This copy of NXT Studio is damaged. Download it again."), 1;
    in.seekg(std::streamoff(fileSize - sizeof t));
    in.read(reinterpret_cast<char *>(&t), sizeof t);
    if (!in || std::memcmp(t.magic, pack::Magic, sizeof t.magic) != 0 || t.compressedSize > fileSize - sizeof t)
        return fail(L"This copy of NXT Studio is damaged. Download it again."), 1;

    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return fail(L"Can't find the AppData folder."), 1;
    const fs::path base = fs::path(local) / L"NXT Studio" / L"app";
    CoTaskMemFree(local);
    wchar_t name[17];
    swprintf(name, 17, L"%016llx", static_cast<unsigned long long>(t.hash));
    const fs::path dir = base / name;
    const fs::path exe = dir / L"NXTStudio.exe";

    std::error_code ec;
    if (!fs::exists(dir / L".ready", ec)) {
        std::vector<unsigned char> payload(t.compressedSize);
        in.seekg(std::streamoff(fileSize - sizeof t - t.compressedSize));
        in.read(reinterpret_cast<char *>(payload.data()), std::streamsize(payload.size()));
        if (!in || pack::fnv1a(payload.data(), payload.size()) != t.hash) return fail(L"This copy of NXT Studio is damaged. Download it again."), 1;

        std::vector<unsigned char> archive(t.rawSize);
        DECOMPRESSOR_HANDLE d = nullptr;
        SIZE_T got = 0;
        const bool ok = CreateDecompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &d) &&
                        Decompress(d, payload.data(), payload.size(), archive.data(), archive.size(), &got) && got == archive.size();
        if (d) CloseDecompressor(d);
        if (!ok) return fail(L"Couldn't unpack NXT Studio. Download it again."), 1;

        // Unpack next to the final folder and rename it into place, so a half-written copy is never used.
        const fs::path tmp = base / (std::wstring(name) + L".tmp" + std::to_wstring(GetCurrentProcessId()));
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp, ec);
        if (!unpack(archive, tmp)) {
            fs::remove_all(tmp, ec);
            return fail(L"Couldn't unpack NXT Studio into " + base.wstring() + L". Is the disk full?"), 1;
        }
        std::ofstream(tmp / L".ready").put('1');
        fs::remove_all(dir, ec);
        fs::rename(tmp, dir, ec);
        if (ec) fs::remove_all(tmp, ec); // another copy started at the same moment finished first

        // Older builds aren't needed any more. A folder that is still in use can't be renamed, so one
        // still running is left alone until next time.
        for (const auto &e : fs::directory_iterator(base, ec)) {
            // Skip ourselves and copies another launch may be unpacking right now.
            if (e.path() == dir || e.path().filename().wstring().find(L".tmp") != std::wstring::npos) continue;
            const fs::path gone = e.path().wstring() + L".old";
            std::error_code rec;
            fs::rename(e.path(), gone, rec);
            if (!rec || e.path().extension() == L".old") fs::remove_all(rec ? e.path() : gone, rec);
        }
    }

    // Pass along whatever came after our own name on the command line.
    std::wstring cmd = L"\"" + exe.wstring() + L"\"";
    const wchar_t *rest = GetCommandLineW();
    if (*rest == L'"') {
        for (++rest; *rest && *rest != L'"'; ++rest) {}
        if (*rest) ++rest;
    } else {
        while (*rest && *rest != L' ' && *rest != L'\t') ++rest;
    }
    while (*rest == L' ' || *rest == L'\t') ++rest;
    if (*rest) cmd += L" " + std::wstring(rest);
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return fail(L"Couldn't start NXT Studio from " + dir.wstring() + L"."), 1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
