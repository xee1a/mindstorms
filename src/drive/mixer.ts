// Turns gamepad sticks/triggers into motor powers. Pure functions, unit tested.

export type DriveMode = 'tank' | 'arcade' | 'racing';
export type OutputIndex = 0 | 1 | 2;

export interface DriveConfig {
  mode: DriveMode;
  leftMotor: OutputIndex;
  rightMotor: OutputIndex;
  /** Third motor (e.g. an arm), driven by the shoulder buttons; null to disable. */
  auxMotor: OutputIndex | null;
  invertLeft: boolean;
  invertRight: boolean;
  invertAux: boolean;
  /** Maximum power 10..100. */
  maxPower: number;
  /** Stick deadzone 0..0.5. */
  deadzone: number;
  /** Response curve 0 (linear) .. 1 (strongly expo). */
  expo: number;
  /** Power for the aux motor while a shoulder button is held. */
  auxPower: number;
}

export const DEFAULT_DRIVE_CONFIG: DriveConfig = {
  mode: 'arcade',
  leftMotor: 1,
  rightMotor: 2,
  auxMotor: 0,
  invertLeft: false,
  invertRight: false,
  invertAux: false,
  maxPower: 100,
  deadzone: 0.12,
  expo: 0.35,
  auxPower: 60,
};

/** Normalised controller input. Stick Y is up-positive (the Gamepad API reports up as -1). */
export interface ControlInput {
  leftX: number;
  leftY: number;
  rightX: number;
  rightY: number;
  /** 0..1 */
  leftTrigger: number;
  rightTrigger: number;
  leftBumper: boolean;
  rightBumper: boolean;
}

export const NEUTRAL_INPUT: ControlInput = {
  leftX: 0,
  leftY: 0,
  rightX: 0,
  rightY: 0,
  leftTrigger: 0,
  rightTrigger: 0,
  leftBumper: false,
  rightBumper: false,
};

const clamp = (v: number, lo = -1, hi = 1) => Math.max(lo, Math.min(hi, v));

/** Scaled radial deadzone: values inside `dz` become 0, the rest are rescaled to keep full range. */
export function applyDeadzone(x: number, y: number, dz: number): [number, number] {
  const mag = Math.hypot(x, y);
  if (mag <= dz || mag === 0) return [0, 0];
  const scaled = Math.min(1, (mag - dz) / (1 - dz));
  return [(x / mag) * scaled, (y / mag) * scaled];
}

/** Blend between linear and cubic response for finer control near centre. */
export function applyExpo(v: number, expo: number): number {
  return (1 - expo) * v + expo * v * v * v;
}

export function mix(input: ControlInput, cfg: DriveConfig): [number, number, number] {
  const [lx, ly] = applyDeadzone(input.leftX, input.leftY, cfg.deadzone);
  const [rx, ry] = applyDeadzone(input.rightX, input.rightY, cfg.deadzone);
  const shape = (v: number) => applyExpo(clamp(v), cfg.expo);

  let left: number;
  let right: number;
  if (cfg.mode === 'tank') {
    left = shape(ly);
    right = shape(ry);
  } else {
    const throttle = cfg.mode === 'racing' ? shape(input.rightTrigger - input.leftTrigger) : shape(ly);
    const steerAxis = cfg.mode === 'racing' ? lx : Math.abs(rx) > Math.abs(lx) ? rx : lx;
    const turn = shape(steerAxis);
    left = throttle + turn;
    right = throttle - turn;
    // Keep the ratio between sides when one exceeds full scale.
    const over = Math.max(1, Math.abs(left), Math.abs(right));
    left /= over;
    right /= over;
  }

  const out: [number, number, number] = [0, 0, 0];
  const scale = clamp(cfg.maxPower, 0, 100);
  out[cfg.leftMotor] += (cfg.invertLeft ? -left : left) * scale;
  if (cfg.rightMotor !== cfg.leftMotor) out[cfg.rightMotor] += (cfg.invertRight ? -right : right) * scale;

  if (cfg.auxMotor !== null && cfg.auxMotor !== cfg.leftMotor && cfg.auxMotor !== cfg.rightMotor) {
    const dir = (input.rightBumper ? 1 : 0) - (input.leftBumper ? 1 : 0);
    out[cfg.auxMotor] = dir * (cfg.invertAux ? -1 : 1) * clamp(cfg.auxPower, 0, 100);
  }

  return out.map((p) => {
    const r = Math.round(p);
    return r === 0 ? 0 : r; // normalise -0
  }) as [number, number, number];
}
