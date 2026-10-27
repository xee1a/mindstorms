# NXT Studio

A Windows desktop app for programming LEGO MINDSTORMS **NXT** (not EV3) bricks.
It has a Scratch-style block editor, inspired by the feel of classic NXT classroom
software but using only original artwork. It also has a live "Drive" mode that reads a
wired USB gamepad on the PC and drives the brick's motors in real time.

## Stack
- Electron (main process: `electron/main.cjs`, plain CommonJS, no build step)
- Vite + TypeScript renderer (`src/`), no UI framework, with hand-written CSS in `src/styles.css`
- Blockly (zelos renderer) for the block editor
- **WebUSB** in the renderer talks to the NXT (VID `0x0694`, PID `0x0002`, bulk EP 0x01 out / 0x82 in).
  The main process grants USB permission for LEGO devices only.
- The browser **Gamepad API** reads XInput/HID controllers, so no native modules are needed.

## NXT driver
WebUSB needs the **WinUSB** driver bound to the NXT. The old LEGO/NI Fantom driver won't work.
Install it once with Zadig: select "LEGO MINDSTORMS NXT" and choose WinUSB. Note that NXT-G/Fantom-based
tools can't see the brick while WinUSB is bound.

## Layout
- `src/nxt/protocol.ts`: pure packet encoders/decoders for NXT direct and system commands (unit tested)
- `src/nxt/transport.ts`: `Transport` interface plus the WebUSB implementation
- `src/nxt/mock.ts`: simulated "demo brick" for testing without hardware
- `src/nxt/brick.ts`: `NxtBrick`, which serialises the command queue, polls telemetry and manages connection state
- `src/drive/`: gamepad polling and the tank/arcade/racing mixer (`mixer.ts` is pure and unit tested)
- `src/blocks/`: Blockly block definitions, theme, toolbox and icons
- `src/views/`: Program, Drive and Brick screens

## Commands
- After `npm install`, if `node_modules/electron/dist` is missing (postinstall scripts didn't run),
  fetch the binary with `node node_modules/electron/install.js`
- `npm run dev`: Vite dev server and Electron with hot reload
- `npm run web`: renderer only, in a browser (Chrome supports WebUSB and Gamepad too)
- `npm test`: Node's built-in test runner on `*.test.ts` (uses Node 24 type stripping, so avoid TS `enum`s
  and parameter properties in tested modules and import with explicit `.ts` extensions)
- `npm run typecheck`

## Milestones
1. ✅ Block editor and gamepad Drive mode over direct commands
2. Blocks → NXC generator → bundled `nbc.exe` → `.rxe` → download to the brick (system commands) and run
3. Polish, Bluetooth (serial COM) transport, installer
