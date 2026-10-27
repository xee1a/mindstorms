// LEGO MINDSTORMS NXT direct/system command packets (USB framing: no length prefix).
// Reference: "LEGO MINDSTORMS NXT Bluetooth Developer Kit", appendix 1 & 2.

export const CommandType = {
  Direct: 0x00,
  System: 0x01,
  Reply: 0x02,
  NoReply: 0x80,
} as const;

export const DirectOp = {
  StartProgram: 0x00,
  StopProgram: 0x01,
  PlayTone: 0x03,
  SetOutputState: 0x04,
  SetInputMode: 0x05,
  GetOutputState: 0x06,
  GetInputValues: 0x07,
  ResetInputScaledValue: 0x08,
  ResetMotorPosition: 0x0a,
  GetBatteryLevel: 0x0b,
  StopSoundPlayback: 0x0c,
  KeepAlive: 0x0d,
  GetCurrentProgramName: 0x11,
} as const;

export const SystemOp = {
  GetFirmwareVersion: 0x88,
  GetDeviceInfo: 0x9b,
} as const;

/** Output mode bits. */
export const OutputMode = {
  MotorOn: 0x01,
  Brake: 0x02,
  Regulated: 0x04,
} as const;

export const RegulationMode = {
  Idle: 0x00,
  Speed: 0x01,
  Sync: 0x02,
} as const;

export const RunState = {
  Idle: 0x00,
  RampUp: 0x10,
  Running: 0x20,
  RampDown: 0x40,
} as const;

export const SensorType = {
  None: 0x00,
  Switch: 0x01,
  Temperature: 0x02,
  Reflection: 0x03,
  Angle: 0x04,
  LightActive: 0x05,
  LightInactive: 0x06,
  SoundDb: 0x07,
  SoundDba: 0x08,
  Custom: 0x09,
  LowSpeed: 0x0a,
  LowSpeed9V: 0x0b,
} as const;

export const SensorMode = {
  Raw: 0x00,
  Boolean: 0x20,
  TransitionCount: 0x40,
  PeriodCounter: 0x60,
  PercentFullScale: 0x80,
  Celsius: 0xa0,
  Fahrenheit: 0xc0,
  AngleSteps: 0xe0,
} as const;

/** Motor port numbers. 0xFF addresses all three outputs. */
export type OutputPort = 0 | 1 | 2 | 0xff;
/** Sensor port numbers 0-3 (labelled 1-4 on the brick). */
export type InputPort = 0 | 1 | 2 | 3;

export const PORT_ALL: OutputPort = 0xff;
export const OUTPUT_LABELS = ['A', 'B', 'C'] as const;

const STATUS_MESSAGES: Record<number, string> = {
  0x20: 'Pending communication transaction in progress',
  0x40: 'Specified mailbox queue is empty',
  0xbd: 'Request failed (i.e. specified file not found)',
  0xbe: 'Unknown command opcode',
  0xbf: 'Insane packet',
  0xc0: 'Data contains out-of-range values',
  0xdd: 'Communication bus error',
  0xde: 'No free memory in communication buffer',
  0xdf: 'Specified channel/connection is not valid',
  0xe0: 'Specified channel/connection not configured or busy',
  0xec: 'No active program',
  0xed: 'Illegal size specified',
  0xee: 'Illegal mailbox queue ID specified',
  0xef: 'Attempted to access invalid field of a structure',
  0xf0: 'Bad input or output specified',
  0xfb: 'Insufficient memory available',
  0xff: 'Bad arguments',
};

export class NxtError extends Error {
  readonly status: number;
  constructor(status: number, op: number) {
    const msg = STATUS_MESSAGES[status] ?? `Unknown error 0x${status.toString(16)}`;
    super(`NXT command 0x${op.toString(16).padStart(2, '0')} failed: ${msg}`);
    this.status = status;
  }
}

const clampInt = (v: number, lo: number, hi: number) => Math.max(lo, Math.min(hi, Math.round(v)));

function direct(op: number, reply: boolean, ...payload: number[]): Uint8Array {
  return Uint8Array.from([reply ? CommandType.Direct : CommandType.Direct | CommandType.NoReply, op, ...payload]);
}

function u16(v: number): number[] {
  return [v & 0xff, (v >> 8) & 0xff];
}

function u32(v: number): number[] {
  return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff];
}

export interface OutputState {
  port: OutputPort;
  /** -100..100 */
  power: number;
  mode: number;
  regulation: number;
  /** -100..100, only used with sync regulation. */
  turnRatio: number;
  runState: number;
  /** Degrees to run before stopping; 0 = run forever. */
  tachoLimit: number;
}

export function setOutputState(s: OutputState, reply = false): Uint8Array {
  return direct(
    DirectOp.SetOutputState,
    reply,
    s.port,
    clampInt(s.power, -100, 100) & 0xff,
    s.mode,
    s.regulation,
    clampInt(s.turnRatio, -100, 100) & 0xff,
    s.runState,
    ...u32(Math.max(0, Math.round(s.tachoLimit))),
  );
}

/** Convenience: spin a motor at `power`, or brake/coast when power is 0. */
export function motorPower(
  port: OutputPort,
  power: number,
  opts: { brake?: boolean; regulated?: boolean } = {},
): Uint8Array {
  const p = clampInt(power, -100, 100);
  if (p === 0) {
    return opts.brake
      ? setOutputState({
          port,
          power: 0,
          mode: OutputMode.MotorOn | OutputMode.Brake | OutputMode.Regulated,
          regulation: RegulationMode.Speed,
          turnRatio: 0,
          runState: RunState.Running,
          tachoLimit: 0,
        })
      : setOutputState({ port, power: 0, mode: 0, regulation: RegulationMode.Idle, turnRatio: 0, runState: RunState.Idle, tachoLimit: 0 });
  }
  const regulated = opts.regulated ?? false;
  return setOutputState({
    port,
    power: p,
    mode: OutputMode.MotorOn | OutputMode.Brake | (regulated ? OutputMode.Regulated : 0),
    regulation: regulated ? RegulationMode.Speed : RegulationMode.Idle,
    turnRatio: 0,
    runState: RunState.Running,
    tachoLimit: 0,
  });
}

export const playTone = (frequencyHz: number, durationMs: number) =>
  direct(DirectOp.PlayTone, false, ...u16(clampInt(frequencyHz, 200, 14000)), ...u16(clampInt(durationMs, 0, 0xffff)));

export const setInputMode = (port: InputPort, type: number, mode: number) =>
  direct(DirectOp.SetInputMode, false, port, type, mode);

export const getInputValues = (port: InputPort) => direct(DirectOp.GetInputValues, true, port);
export const getOutputState = (port: 0 | 1 | 2) => direct(DirectOp.GetOutputState, true, port);
export const resetMotorPosition = (port: 0 | 1 | 2, relative = false) =>
  direct(DirectOp.ResetMotorPosition, false, port, relative ? 1 : 0);
export const getBatteryLevel = () => direct(DirectOp.GetBatteryLevel, true);
export const keepAlive = () => direct(DirectOp.KeepAlive, true);
export const stopProgram = () => direct(DirectOp.StopProgram, false);
export const stopSound = () => direct(DirectOp.StopSoundPlayback, false);
export const getFirmwareVersion = () => Uint8Array.from([CommandType.System, SystemOp.GetFirmwareVersion]);
export const getDeviceInfo = () => Uint8Array.from([CommandType.System, SystemOp.GetDeviceInfo]);

/** Returns true when the packet asks the brick for a reply. */
export function expectsReply(packet: Uint8Array): boolean {
  return (packet[0] & CommandType.NoReply) === 0;
}

/** Validates a reply's header and status byte and returns a DataView over it. */
export function checkReply(reply: Uint8Array, op: number): DataView {
  if (reply.length < 3 || reply[0] !== CommandType.Reply || reply[1] !== op) {
    throw new Error(`Unexpected reply for 0x${op.toString(16)}: [${Array.from(reply.slice(0, 4)).join(', ')}]`);
  }
  if (reply[2] !== 0) throw new NxtError(reply[2], op);
  return new DataView(reply.buffer, reply.byteOffset, reply.byteLength);
}

export function parseBatteryLevel(reply: Uint8Array): number {
  return checkReply(reply, DirectOp.GetBatteryLevel).getUint16(3, true);
}

export interface InputValues {
  port: InputPort;
  valid: boolean;
  calibrated: boolean;
  type: number;
  mode: number;
  raw: number;
  normalized: number;
  scaled: number;
}

export function parseInputValues(reply: Uint8Array): InputValues {
  const v = checkReply(reply, DirectOp.GetInputValues);
  return {
    port: v.getUint8(3) as InputPort,
    valid: v.getUint8(4) !== 0,
    calibrated: v.getUint8(5) !== 0,
    type: v.getUint8(6),
    mode: v.getUint8(7),
    raw: v.getUint16(8, true),
    normalized: v.getUint16(10, true),
    scaled: v.getInt16(12, true),
  };
}

export interface OutputStateReply {
  port: number;
  power: number;
  mode: number;
  regulation: number;
  turnRatio: number;
  runState: number;
  tachoLimit: number;
  tachoCount: number;
  blockTachoCount: number;
  rotationCount: number;
}

export function parseOutputState(reply: Uint8Array): OutputStateReply {
  const v = checkReply(reply, DirectOp.GetOutputState);
  return {
    port: v.getUint8(3),
    power: v.getInt8(4),
    mode: v.getUint8(5),
    regulation: v.getUint8(6),
    turnRatio: v.getInt8(7),
    runState: v.getUint8(8),
    tachoLimit: v.getUint32(9, true),
    tachoCount: v.getInt32(13, true),
    blockTachoCount: v.getInt32(17, true),
    rotationCount: v.getInt32(21, true),
  };
}

export interface FirmwareVersion {
  protocol: string;
  firmware: string;
}

export function parseFirmwareVersion(reply: Uint8Array): FirmwareVersion {
  const v = checkReply(reply, SystemOp.GetFirmwareVersion);
  return {
    protocol: `${v.getUint8(4)}.${v.getUint8(3)}`,
    firmware: `${v.getUint8(6)}.${v.getUint8(5).toString().padStart(2, '0')}`,
  };
}

export interface DeviceInfo {
  name: string;
  bluetoothAddress: string;
  freeFlash: number;
}

export function parseDeviceInfo(reply: Uint8Array): DeviceInfo {
  const v = checkReply(reply, SystemOp.GetDeviceInfo);
  const nameBytes = reply.slice(3, 18);
  const end = nameBytes.indexOf(0);
  const name = new TextDecoder().decode(end >= 0 ? nameBytes.slice(0, end) : nameBytes);
  const bt = Array.from(reply.slice(18, 24), (b) => b.toString(16).padStart(2, '0')).join(':');
  return { name, bluetoothAddress: bt, freeFlash: v.getUint32(29, true) };
}
