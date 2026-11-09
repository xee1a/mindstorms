# NXT Studio

A native Windows desktop app for programming LEGO MINDSTORMS **NXT** (not EV3) bricks.
It has a Scratch-style block editor, inspired by the feel of classic NXT classroom
software but using only original artwork. A wired USB gamepad on the PC is an input for
programs (Gamepad blocks); the Controller (Pad) tab only shows what it sends, it drives nothing itself.
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
- `src/nxt/Transport.h`: `Transport` interface, implemented by `WinUsbTransport` and `SerialTransport` (Bluetooth COM port)
- `src/nxt/Brick.*`: `BrickWorker` runs on its own thread and owns the transport, coalesces motor commands,
  polls telemetry and auto-reconnects. It also uploads programs, lists files, does the factory reset and
  streams the gamepad into inbox 10 while a program runs on the brick. It also renames the brick, and after a Bluetooth
  connection uploads and runs `NXT Studio.rxe`, a 2.5 s "connected over Bluetooth" screen that MainWindow compiles at startup
  in the UI language (skipped while another program runs). `Brick::instance()` is the UI-side facade.
- `src/nxt/SystemFiles.*`: copies of every sound file and LEGO's Try Me programs, read from each brick over USB into
  `%APPDATA%\NXT Studio\NXT Studio\brick-files`; a normal factory reset keeps them and writes back any that are missing (while they fit). The three firmware
  sounds also have synthesised stand-ins (never bundle LEGO's files themselves)
- `src/drive/Mixer.*`: `ControlInput` plus a pure tank/arcade/racing mixer (unit tested). `Gamepad.*` handles XInput/DirectInput
  polling, including spotting generic pads in digital mode (ANALOG light off).
  `PadLink.*` encodes the pad for programs on the brick.
- `src/compile/`: `NxcGenerator` turns a workspace into NXC (unit tested: every block compiles with nbc);
  `Compiler` runs the bundled `third_party/nbc/nbc.exe`. Generated helpers use the `ns_` prefix, since NXC's
  own headers define many `__` macros.
  - HiTechnic gyro (`nxt_gyro_*` blocks, `SensorKind::Gyro`): an analog sensor read raw in light-inactive mode, rate = raw − resting
    value (~1 °/s per unit). The program calibrates each gyro at start and a task integrates the angle; Runner does the same from
    telemetry (coarser, ~120 ms over USB)
- `src/blocks/`: the custom block editor on QGraphicsView
  - `BlockDefs` defines the blocks
  - `Block` handles layout, paint and JSON
  - `Workspace` handles snapping, undo and variables
  - `Canvas` is the view
  - `Palette` holds the categories and flyout
- `src/run/Runner.*`: runs a block program from the PC (one thread per stack), sending direct commands live;
  degree moves are braked by `BrickWorker::pollMoves` on the USB thread
- `src/views/`: Program, Controller and Brick screens, and `UploadDialog`. Controller shows the pad with every input's live
  value, named as in the Gamepad blocks; all control is done by programs. Its dropdown picks the look (`PadView::Layout`):
  the MANGAN 200 picture or generic PlayStation/Xbox drawings, saved in `Prefs::padLayout`.
- `src/app/`: MainWindow, Splash, Theme (dark/light tokens plus accent hue shift), Prefs (live settings with a `changed` signal),
  SettingsDialog, FactoryResetDialog, Sounds (editor clicks, synthesised), shared widgets (`BrickArt`: the brick picture
  `resources/images/nxt.png` with its name and battery on the screen; `PadView`: the MANGAN 200 picture or a drawn layout, with per-layout hotspots; `BatteryWidget`)
- `i18n/nxtstudio_pl.ts`: Polish translation. Source strings are English.
- `resources/`: SVG icons, the app icon, `images/nxt.png` and `images/pad_mangan200.png` (the user's pictures, background removed)

## Commands
- `.\build.ps1`: configure, build Release, run tests, then copy a runnable folder to `dist\` (windeployqt)
  - Release also writes `release\NXTStudio.exe`: `tools/single-exe` (`nxtlauncher` + `nxtpack`) appends dist\ LZMS-compressed
    to a static-CRT launcher that unpacks to `%LOCALAPPDATA%\NXT Studio\app\<hash>` once per build
  - and `release\NXTStudio.msi` via `tools/make_msi.py` (Python 3.12 `msilib`; keep its `UPGRADE_CODE` fixed so upgrades replace old installs)
  - Both files under `release\` are committed so people can download without building
- `.\build.ps1 -Debug`: Debug build
- `cmake --build build --target update_translations`: refresh the .ts file after changing UI strings
  (then translate the new entries)

## Conventions
- Never use `slots`/`signals` as identifiers, because they're Qt macros
- `QString::arg(uint8_t)` formats a character, so cast to `int`
- Wrap all user-visible text in `tr()`, and keep the Polish .ts with 0 unfinished entries. Write natural Polish
  ("bloczek", "skrypt", "podczas gdy"), not word-for-word translation
- Outside QObject classes, use a file-local `struct Text { Q_DECLARE_TR_FUNCTIONS(Ctx) };` and `Text::tr()`, so lupdate
  and the runtime agree on the context
- Colours come from `theme::` tokens or the dark-theme QSS (Theme.cpp maps each QSS colour to a light one), never new literals
- Theme, accent and language apply on restart; other settings go through `Prefs` and apply live
- Commit messages carry no Claude/AI co-author trailer
- Settings are stored with QSettings (`HKCU\Software\NXT Studio\NXT Studio`)
- The Electron prototype is preserved at git tag `electron-m1`

## Milestones
1. ✅ Block editor and gamepad Drive mode over direct commands (native Qt rewrite)
2. ✅ Run from PC over USB (Program → Run), and Upload to NXT (blocks → NXC → bundled `nbc.exe` → `.rxe` → system commands)
3. ✅ Bluetooth (serial COM) transport, single-file exe and MSI installer
