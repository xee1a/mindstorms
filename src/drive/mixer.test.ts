import { test } from 'node:test';
import assert from 'node:assert/strict';
import { applyDeadzone, DEFAULT_DRIVE_CONFIG, mix, NEUTRAL_INPUT, type DriveConfig } from './mixer.ts';

const linear: DriveConfig = { ...DEFAULT_DRIVE_CONFIG, expo: 0, deadzone: 0 };

test('deadzone zeroes small input and rescales the rest', () => {
  assert.deepEqual(applyDeadzone(0.05, 0.05, 0.1), [0, 0]);
  const [x, y] = applyDeadzone(1, 0, 0.2);
  assert.equal(x, 1);
  assert.equal(y, 0);
  const [hx] = applyDeadzone(0.6, 0, 0.2);
  assert.ok(Math.abs(hx - 0.5) < 1e-9);
});

test('neutral input stops every motor', () => {
  assert.deepEqual(mix(NEUTRAL_INPUT, DEFAULT_DRIVE_CONFIG), [0, 0, 0]);
});

test('arcade: full forward drives both sides forward', () => {
  assert.deepEqual(mix({ ...NEUTRAL_INPUT, leftY: 1 }, linear), [0, 100, 100]);
});

test('arcade: pure turn spins in place', () => {
  assert.deepEqual(mix({ ...NEUTRAL_INPUT, rightX: 1 }, linear), [0, 100, -100]);
});

test('arcade: forward + turn keeps ratio within range', () => {
  const [, l, r] = mix({ ...NEUTRAL_INPUT, leftY: 1, rightX: 0.5 }, linear);
  assert.equal(l, 100);
  assert.equal(r, 33);
});

test('tank: each stick drives its own side', () => {
  assert.deepEqual(mix({ ...NEUTRAL_INPUT, leftY: 1, rightY: -0.5 }, { ...linear, mode: 'tank' }), [0, 100, -50]);
});

test('racing: triggers set throttle', () => {
  const cfg: DriveConfig = { ...linear, mode: 'racing' };
  assert.deepEqual(mix({ ...NEUTRAL_INPUT, rightTrigger: 1 }, cfg), [0, 100, 100]);
  assert.deepEqual(mix({ ...NEUTRAL_INPUT, leftTrigger: 0.5 }, cfg), [0, -50, -50]);
});

test('max power, inversion and port mapping', () => {
  const cfg: DriveConfig = { ...linear, maxPower: 50, leftMotor: 2, rightMotor: 0, invertRight: true, auxMotor: 1 };
  assert.deepEqual(mix({ ...NEUTRAL_INPUT, leftY: 1, rightBumper: true }, cfg), [-50, 60, 50]);
});

test('aux motor is ignored when it collides with a drive motor', () => {
  const cfg: DriveConfig = { ...linear, auxMotor: 1 };
  assert.deepEqual(mix({ ...NEUTRAL_INPUT, leftBumper: true }, cfg), [0, 0, 0]);
});
