# NXT Studio

A native Windows desktop app for programming LEGO MINDSTORMS **NXT** (not EV3) bricks.
It has a Scratch-style block editor, inspired by the feel of classic NXT classroom
software but using only original artwork. It also has a live "Drive" (Jazda) mode that reads a
wired USB gamepad on the PC and drives the brick's motors in real time.
The UI is **Polish by default**, with English available under Settings → Language.

## Stack
- C++20 and **Qt 6.8** (Widgets, Svg, LinguistTools, Test), built with CMake and MSVC 2022 (x64)
- Qt is installed with aqt at `C:\Qt\6.8.3\msvc2022_64`; CMake ships with Visual Studio
  (`...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`), so put it on `PATH`
- USB goes through **WinUSB** (SetupAPI plus `WinUsb_*`): VID `0x0694`, PID `0x0002`, bulk EP 0x01 out / 0x82 in
- Gamepads are read with **XInput**
- The dark theme is Fusion plus QSS in `src/app/Theme.cpp`, with a dark DWM title bar

## NXT driver
The NXT needs the **WinUSB** driver bound to it; the LEGO/NI Fantom driver won't work. Install it once with Zadig:
choose "LEGO MINDSTORMS NXT", then WinUSB. NXT-G/Fantom tools can't see the brick while WinUSB is bound.
The Brick (Kostka) tab explains this to users.

## Layout
- `src/nxt/Protocol.*`: pure packet encoders/decoders for direct and system commands (unit tested)
- `src/nxt/Transport.h`: `Transport` interface; `WinUsbTransport` is the real one, `DemoTransport` a simulated brick
- `src/nxt/Brick.*`: `BrickWorker` runs on its own thread and owns the transport, coalesces motor commands,
  polls telemetry and auto-reconnects. `Brick::instance()` is the UI-side facade.
- `src/drive/Mixer.*`: pure tank/arcade/racing mixer (unit tested). `Gamepad.*` handles XInput polling.
- `src/blocks/`: the custom block editor on QGraphicsView
  - `BlockDefs` defines the blocks
  - `Block` handles layout, paint and JSON
  - `Workspace` handles snapping, undo and variables
  - `Canvas` is the view
  - `Palette` holds the categories and flyout
- `src/views/`: Program, Drive and Brick screens. `src/app/`: MainWindow, Theme, shared widgets.
- `i18n/nxtstudio_pl.ts`: Polish translation. Source strings are English.
- `resources/`: SVG icons, app icon, `images/nxt.png` (the transparent brick photo)
- `tools/cutout.cpp`: one-off tool that turns `tools/source/nxt.jpg` (on white) into `resources/images/nxt.png`

## Commands
- `.\build.ps1`: configure, build Release, run tests, then copy a runnable folder to `dist\` (windeployqt)
- `.\build.ps1 -Debug`: Debug build
- `dist\NXTStudio.exe --demo`: start connected to the simulated brick
- `cmake --build build --target update_translations`: refresh the .ts file after changing UI strings
  (then translate the new entries)
- `cmake --build build --config Release --target cutout`: build the image tool

## Conventions
- Never use `slots`/`signals` as identifiers, because they're Qt macros
- `QString::arg(uint8_t)` formats a character, so cast to `int`
- Wrap all user-visible text in `tr()`, and keep the Polish .ts with 0 unfinished entries
- Settings are stored with QSettings (`HKCU\Software\NXT Studio\NXT Studio`)
- The Electron prototype is preserved at git tag `electron-m1`

## Milestones
1. ✅ Block editor and gamepad Drive mode over direct commands (native Qt rewrite)
2. Blocks → NXC generator → bundled `nbc.exe` → `.rxe` → download to the brick (system commands) and run
3. Bluetooth (serial COM) transport, installer
