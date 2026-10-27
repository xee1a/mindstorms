# NXT Studio

A native Windows app for programming LEGO MINDSTORMS **NXT** bricks. It has
Scratch-style blocks and a live gamepad Drive mode. The interface is in Polish, with English
available in the settings.

- **Program:** build programs from snap-together blocks (motors, sensors, sound, loops, variables)
- **Drive (Jazda):** drive the robot in real time with a wired USB/XInput controller or the keyboard
  (tank, arcade or racing control, plus live sensor readouts)
- **Brick (Kostka):** connection status, battery, firmware info and a driver setup guide

Downloading block programs to the brick is the next milestone.

## Connecting the NXT
The app talks to the brick through the Windows **WinUSB** driver. Set it up once:

1. Connect the NXT over USB and turn it on.
2. Run [Zadig](https://zadig.akeo.ie), then choose *Options → List All Devices* and **LEGO MINDSTORMS NXT**.
3. Choose **WinUSB** and click *Install Driver*.

To try the app without a brick, click "Wypróbuj kostkę demo" on the Kostka tab or start it with `--demo`.

## Building
Requirements:
- Visual Studio 2022 (C++ workload)
- Qt 6.8 for MSVC 2022 x64 at `C:\Qt\6.8.3\msvc2022_64`, for example `aqt install-qt windows desktop 6.8.3 win64_msvc2022_64`

```powershell
.\build.ps1
```

This builds, runs the tests and puts a ready-to-run `dist\NXTStudio.exe` with its Qt DLLs.
