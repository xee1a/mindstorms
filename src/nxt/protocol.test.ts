import { test } from 'node:test';
import assert from 'node:assert/strict';
import * as P from './protocol.ts';

const bytes = (u: Uint8Array) => Array.from(u);

test('motorPower encodes a no-reply SETOUTPUTSTATE', () => {
  assert.deepEqual(bytes(P.motorPower(1, 75)), [0x80, 0x04, 1, 75, 0x03, 0x00, 0, 0x20, 0, 0, 0, 0]);
});

test('negative power is two’s complement and speed regulation sets the mode bits', () => {
  assert.deepEqual(bytes(P.motorPower(0, -100, { regulated: true })), [0x80, 0x04, 0, 0x9c, 0x07, 0x01, 0, 0x20, 0, 0, 0, 0]);
});

test('zero power brakes or coasts', () => {
  assert.deepEqual(bytes(P.motorPower(0xff, 0, { brake: true })), [0x80, 0x04, 0xff, 0, 0x07, 0x01, 0, 0x20, 0, 0, 0, 0]);
  assert.deepEqual(bytes(P.motorPower(2, 0)), [0x80, 0x04, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
});

test('tacho limit is little-endian u32', () => {
  const pkt = P.setOutputState({ port: 0, power: 50, mode: 1, regulation: 0, turnRatio: 0, runState: 0x20, tachoLimit: 360 });
  assert.deepEqual(bytes(pkt.slice(8)), [0x68, 0x01, 0, 0]);
});

test('playTone clamps and encodes u16 LE', () => {
  assert.deepEqual(bytes(P.playTone(440, 500)), [0x80, 0x03, 0xb8, 0x01, 0xf4, 0x01]);
});

test('expectsReply follows the 0x80 bit', () => {
  assert.equal(P.expectsReply(P.getBatteryLevel()), true);
  assert.equal(P.expectsReply(P.playTone(440, 100)), false);
  assert.equal(P.expectsReply(P.getFirmwareVersion()), true);
});

test('parses battery level', () => {
  assert.equal(P.parseBatteryLevel(Uint8Array.from([0x02, 0x0b, 0x00, 0x8c, 0x1e])), 7820);
});

test('error status throws NxtError', () => {
  assert.throws(() => P.parseBatteryLevel(Uint8Array.from([0x02, 0x0b, 0xec, 0, 0])), P.NxtError);
});

test('parses input values', () => {
  const r = Uint8Array.from([0x02, 0x07, 0x00, 2, 1, 0, 0x05, 0x80, 0x10, 0x02, 0x00, 0x02, 0x37, 0x00, 0, 0]);
  const v = P.parseInputValues(r);
  assert.equal(v.port, 2);
  assert.equal(v.valid, true);
  assert.equal(v.type, P.SensorType.LightActive);
  assert.equal(v.raw, 0x210);
  assert.equal(v.scaled, 55);
});

test('parses output state with negative rotation', () => {
  const r = new Uint8Array(25);
  r.set([0x02, 0x06, 0x00, 1, 0xce]);
  new DataView(r.buffer).setInt32(21, -720, true);
  const s = P.parseOutputState(r);
  assert.equal(s.power, -50);
  assert.equal(s.rotationCount, -720);
});

test('parses firmware version and device info', () => {
  assert.deepEqual(P.parseFirmwareVersion(Uint8Array.from([0x02, 0x88, 0, 124, 1, 31, 1])), { protocol: '1.124', firmware: '1.31' });
  const info = new Uint8Array(33);
  info.set([0x02, 0x9b, 0]);
  info.set(new TextEncoder().encode('ROVER'), 3);
  info.set([0x00, 0x16, 0x53, 0x01, 0x02, 0x03], 18);
  new DataView(info.buffer).setUint32(29, 1234, true);
  assert.deepEqual(P.parseDeviceInfo(info), { name: 'ROVER', bluetoothAddress: '00:16:53:01:02:03', freeFlash: 1234 });
});
