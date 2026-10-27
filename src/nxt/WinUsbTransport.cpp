#include "WinUsbTransport.h"

#include <QCoreApplication>
#include <QString>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <setupapi.h>
#include <winusb.h>

namespace nxt {

namespace {

std::runtime_error userError(const QString &message) { return std::runtime_error(message.toStdString()); }

/** Reads a REG_SZ / REG_MULTI_SZ device property as a list of strings. */
std::vector<std::wstring> deviceStrings(HDEVINFO devs, SP_DEVINFO_DATA *info, DWORD prop)
{
    wchar_t buf[2048] = {};
    DWORD type = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(devs, info, prop, &type, reinterpret_cast<BYTE *>(buf), sizeof(buf) - sizeof(wchar_t) * 2, nullptr))
        return {};
    std::vector<std::wstring> out;
    for (const wchar_t *p = buf; *p; p += wcslen(p) + 1) {
        out.emplace_back(p);
        if (type != REG_MULTI_SZ) break;
    }
    return out;
}

bool isNxt(const std::vector<std::wstring> &hardwareIds)
{
    for (const auto &id : hardwareIds) {
        const QString s = QString::fromStdWString(id).toUpper();
        if (s.contains(QStringLiteral("VID_0694&PID_0002"))) return true;
    }
    return false;
}

struct Found {
    WinUsbTransport::Presence presence = WinUsbTransport::Presence::NotFound;
    QString service;
    GUID interfaceGuid{};
    bool hasGuid = false;
};

/** Finds the NXT among present USB devices, and the interface GUID WinUSB registered for it. */
Found findNxt()
{
    Found found;
    HDEVINFO devs = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devs == INVALID_HANDLE_VALUE) return found;

    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(devs, i, &info); ++i) {
        if (!isNxt(deviceStrings(devs, &info, SPDRP_HARDWAREID))) continue;
        const auto service = deviceStrings(devs, &info, SPDRP_SERVICE);
        found.service = service.empty() ? QString() : QString::fromStdWString(service.front());
        if (found.service.compare(QStringLiteral("WinUSB"), Qt::CaseInsensitive) != 0) {
            found.presence = WinUsbTransport::Presence::NeedsDriver;
            continue; // keep looking in case a second NXT is ready
        }
        found.presence = WinUsbTransport::Presence::Ready;

        // Zadig/libwdi registers a per-install interface GUID under the device's hardware key.
        HKEY key = SetupDiOpenDevRegKey(devs, &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (key != INVALID_HANDLE_VALUE) {
            for (const wchar_t *name : {L"DeviceInterfaceGUIDs", L"DeviceInterfaceGUID"}) {
                wchar_t value[512] = {};
                DWORD size = sizeof(value) - sizeof(wchar_t) * 2;
                DWORD type = 0;
                if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE *>(value), &size) == ERROR_SUCCESS &&
                    SUCCEEDED(CLSIDFromString(value, &found.interfaceGuid))) {
                    found.hasGuid = true;
                    break;
                }
            }
            RegCloseKey(key);
        }
        break;
    }
    SetupDiDestroyDeviceInfoList(devs);
    return found;
}

std::wstring devicePath(const GUID &guid)
{
    std::wstring path;
    HDEVINFO devs = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devs == INVALID_HANDLE_VALUE) return path;
    SP_DEVICE_INTERFACE_DATA iface{};
    iface.cbSize = sizeof(iface);
    for (DWORD i = 0; path.empty() && SetupDiEnumDeviceInterfaces(devs, nullptr, &guid, i, &iface); ++i) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(devs, &iface, nullptr, 0, &needed, nullptr);
        std::vector<BYTE> buf(needed);
        auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (SetupDiGetDeviceInterfaceDetailW(devs, &iface, detail, needed, nullptr, nullptr)) {
            const QString p = QString::fromWCharArray(detail->DevicePath).toUpper();
            if (p.contains(QStringLiteral("VID_0694&PID_0002"))) path = detail->DevicePath;
        }
    }
    SetupDiDestroyDeviceInfoList(devs);
    return path;
}

bool isDisconnect(DWORD err)
{
    return err == ERROR_DEVICE_NOT_CONNECTED || err == ERROR_BAD_COMMAND || err == ERROR_GEN_FAILURE ||
           err == ERROR_INVALID_HANDLE || err == ERROR_FILE_NOT_FOUND || err == ERROR_NO_SUCH_DEVICE;
}

} // namespace

WinUsbTransport::Probe WinUsbTransport::probe()
{
    const Found f = findNxt();
    return {f.presence, f.service};
}

std::unique_ptr<WinUsbTransport> WinUsbTransport::open()
{
    const Found f = findNxt();
    if (f.presence == Presence::NotFound)
        throw userError(QCoreApplication::translate("Usb", "No NXT found. Check the USB cable and switch the brick on."));
    if (f.presence == Presence::NeedsDriver)
        throw userError(QCoreApplication::translate("Usb", "The NXT is using the \"%1\" driver. Install WinUSB for it (Brick → Driver setup).")
                            .arg(f.service.isEmpty() ? QCoreApplication::translate("Usb", "none") : f.service));
    if (!f.hasGuid) throw userError(QCoreApplication::translate("Usb", "WinUSB is installed for the NXT, but Windows didn't register its interface. Re-run Zadig."));

    const std::wstring path = devicePath(f.interfaceGuid);
    if (path.empty()) throw userError(QCoreApplication::translate("Usb", "The NXT disappeared while connecting. Try again."));

    std::unique_ptr<WinUsbTransport> t(new WinUsbTransport);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        throw userError(err == ERROR_ACCESS_DENIED
                            ? QCoreApplication::translate("Usb", "Another program (NXT-G, BricxCC…) is using the NXT. Close it and try again.")
                            : QCoreApplication::translate("Usb", "Windows couldn't open the NXT (error %1).").arg(err));
    }
    t->m_file = file;

    WINUSB_INTERFACE_HANDLE wu = nullptr;
    if (!WinUsb_Initialize(file, &wu)) throw userError(QCoreApplication::translate("Usb", "WinUSB couldn't start for the NXT (error %1).").arg(GetLastError()));
    t->m_winusb = wu;

    USB_INTERFACE_DESCRIPTOR desc{};
    if (WinUsb_QueryInterfaceSettings(wu, 0, &desc)) {
        for (UCHAR i = 0; i < desc.bNumEndpoints; ++i) {
            WINUSB_PIPE_INFORMATION pipe{};
            if (!WinUsb_QueryPipe(wu, 0, i, &pipe) || pipe.PipeType != UsbdPipeTypeBulk) continue;
            if (USB_ENDPOINT_DIRECTION_IN(pipe.PipeId)) t->m_inPipe = pipe.PipeId;
            else t->m_outPipe = pipe.PipeId;
        }
    }
    ULONG writeTimeout = 1000;
    WinUsb_SetPipePolicy(wu, t->m_outPipe, PIPE_TRANSFER_TIMEOUT, sizeof(writeTimeout), &writeTimeout);
    WinUsb_FlushPipe(wu, t->m_inPipe);
    return t;
}

WinUsbTransport::~WinUsbTransport()
{
    if (m_winusb) WinUsb_Free(static_cast<WINUSB_INTERFACE_HANDLE>(m_winusb));
    if (m_file) CloseHandle(static_cast<HANDLE>(m_file));
}

void WinUsbTransport::write(const QByteArray &packet)
{
    ULONG written = 0;
    QByteArray copy = packet; // WinUsb_WritePipe takes a non-const buffer
    if (!WinUsb_WritePipe(static_cast<WINUSB_INTERFACE_HANDLE>(m_winusb), m_outPipe,
                          reinterpret_cast<PUCHAR>(copy.data()), static_cast<ULONG>(copy.size()), &written, nullptr)) {
        const DWORD err = GetLastError();
        if (isDisconnect(err)) throw DisconnectedError(QCoreApplication::translate("Usb", "The NXT was disconnected.").toStdString());
        if (err == ERROR_SEM_TIMEOUT) throw TimeoutError();
        throw std::runtime_error(QCoreApplication::translate("Usb", "USB write failed (error %1).").arg(err).toStdString());
    }
}

QByteArray WinUsbTransport::read(int timeoutMs)
{
    auto *wu = static_cast<WINUSB_INTERFACE_HANDLE>(m_winusb);
    if (timeoutMs != m_currentTimeout) {
        ULONG t = static_cast<ULONG>(timeoutMs);
        WinUsb_SetPipePolicy(wu, m_inPipe, PIPE_TRANSFER_TIMEOUT, sizeof(t), &t);
        m_currentTimeout = timeoutMs;
    }
    QByteArray buf(64, '\0');
    ULONG got = 0;
    // A timed-out transfer is cancelled by WinUSB, so no stale reply is left behind.
    if (!WinUsb_ReadPipe(wu, m_inPipe, reinterpret_cast<PUCHAR>(buf.data()), static_cast<ULONG>(buf.size()), &got, nullptr)) {
        const DWORD err = GetLastError();
        if (err == ERROR_SEM_TIMEOUT) throw TimeoutError();
        if (isDisconnect(err)) throw DisconnectedError(QCoreApplication::translate("Usb", "The NXT was disconnected.").toStdString());
        throw std::runtime_error(QCoreApplication::translate("Usb", "USB read failed (error %1).").arg(err).toStdString());
    }
    buf.truncate(static_cast<qsizetype>(got));
    return buf;
}

} // namespace nxt
