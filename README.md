# NXT Studio

A native Windows app for programming LEGO MINDSTORMS **NXT** bricks. It has
Scratch-style blocks, with a USB gamepad as an input for programs. The interface is in Polish, with English,
a light theme and accent colours available in the settings.

- **Program:** build programs from snap-together blocks (motors, sensors, sound, loops, variables,
  gamepad and Bluetooth mailboxes). **Run** tries a program live from the PC; **Upload to NXT** compiles
  it and saves it on the brick under a name you choose, so it runs without the PC.
- **Controller (Pad):** shows live what the plugged-in pad sends, named as in the Gamepad blocks. It controls
  nothing by itself: programs decide what each button and stick does.
- **Brick (Kostka):** connection over USB or Bluetooth, battery, firmware info, a driver setup guide,
  and a factory reset (Settings → Brick) that clears the files on the brick

Programs on the brick that use gamepad blocks read the PC's pad while the app stays connected.

## Download
No need to build it yourself. In the [`release`](release) folder:
- **`NXTStudio.msi`**: the installer. Double-click it; it puts NXT Studio in Program Files with Start menu and
  desktop shortcuts. Running a newer `.msi` replaces the old version; uninstall from Settings → Apps.
- **`NXTStudio.exe`**: the whole app as one file, no install. The first start unpacks it to
  `%LOCALAPPDATA%\NXT Studio\app` (takes a second); later starts are instant.

Windows SmartScreen may warn because the files aren't signed: click *More info → Run anyway*.

## Uploading
Block programs are turned into NXC and compiled by the bundled NBC compiler
(`third_party/nbc`, copied to `tools\nbc.exe` next to the app). This works with the standard LEGO
firmware 1.28 or newer.

## Connecting the NXT
The app talks to the brick through the Windows **WinUSB** driver. Set it up once:

1. Connect the NXT over USB and turn it on.
2. Run [Zadig](https://zadig.akeo.ie), then choose *Options → List All Devices* and **LEGO MINDSTORMS NXT**.
3. Choose **WinUSB** and click *Install Driver*.

For Bluetooth, pair the NXT with Windows (code 1234), then pick its outgoing COM port on the Brick tab.

## Building
Requirements:
- Visual Studio 2022 (C++ workload)
- Qt 6.8 for MSVC 2022 x64 at `C:\Qt\6.8.3\msvc2022_64`, for example `aqt install-qt windows desktop 6.8.3 win64_msvc2022_64`

```powershell
.\build.ps1
```

This builds, runs the tests and puts a ready-to-run `dist\NXTStudio.exe` with its Qt DLLs, then makes
`release\NXTStudio.exe` (one file) and, if Python 3.12 is installed, `release\NXTStudio.msi`.
