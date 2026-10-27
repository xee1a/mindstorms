import * as P from './protocol.ts';
import { DemoTransport } from './mock.ts';
import { LEGO_VENDOR_ID, NXT_PRODUCT_ID, type Transport, WebUsbTransport } from './transport.ts';

export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected';

export type SensorKind = 'none' | 'touch' | 'light' | 'ambient' | 'sound';

export const SENSOR_KINDS: Record<SensorKind, { label: string; type: number; mode: number; unit: string }> = {
  none: { label: 'Not used', type: P.SensorType.None, mode: P.SensorMode.Raw, unit: '' },
  touch: { label: 'Touch', type: P.SensorType.Switch, mode: P.SensorMode.Boolean, unit: '' },
  light: { label: 'Light (reflected)', type: P.SensorType.LightActive, mode: P.SensorMode.PercentFullScale, unit: '%' },
  ambient: { label: 'Light (ambient)', type: P.SensorType.LightInactive, mode: P.SensorMode.PercentFullScale, unit: '%' },
  sound: { label: 'Sound', type: P.SensorType.SoundDb, mode: P.SensorMode.PercentFullScale, unit: 'dB' },
};

export interface BrickState {
  status: ConnectionStatus;
  transport: 'usb' | 'demo' | null;
  name: string;
  firmware: string;
  bluetoothAddress: string;
  freeFlash: number;
  batteryMv: number | null;
  /** Requested power per output A/B/C. */
  motorPower: [number, number, number];
  /** Rotation in degrees per output A/B/C, from the brick. */
  motorRotation: [number, number, number];
  sensors: SensorKind[];
  sensorValues: (number | null)[];
  lastError: string | null;
}

const SENSOR_STORAGE_KEY = 'nxt-studio.sensors';
const TELEMETRY_INTERVAL_MS = 120;
const BATTERY_INTERVAL_MS = 10_000;
const KEEPALIVE_INTERVAL_MS = 60_000;
const MOTOR_REFRESH_MS = 1_000;
const REPLY_TIMEOUT_MS = 800;

function loadSensors(): SensorKind[] {
  try {
    const raw = JSON.parse(localStorage.getItem(SENSOR_STORAGE_KEY) ?? 'null');
    if (Array.isArray(raw) && raw.length === 4 && raw.every((k) => k in SENSOR_KINDS)) return raw;
  } catch {
    /* ignore */
  }
  return ['touch', 'sound', 'light', 'none'];
}

type Listener = (s: BrickState) => void;

/** Owns the link to one brick: serialises commands, coalesces motor updates and polls telemetry. */
export class NxtBrick {
  private transport: Transport | null = null;
  private chain: Promise<unknown> = Promise.resolve();
  private listeners = new Set<Listener>();
  private telemetryTimer: ReturnType<typeof setInterval> | null = null;
  private telemetryBusy = false;
  private lastBattery = 0;
  private lastKeepAlive = 0;
  private failures = 0;

  private desiredPower: [number, number, number] = [0, 0, 0];
  private sentPower: [number | null, number | null, number | null] = [null, null, null];
  private lastMotorSend = 0;
  private motorFlushQueued = false;
  brakeOnStop = true;
  speedRegulation = false;

  state: BrickState = {
    status: 'disconnected',
    transport: null,
    name: '',
    firmware: '',
    bluetoothAddress: '',
    freeFlash: 0,
    batteryMv: null,
    motorPower: [0, 0, 0],
    motorRotation: [0, 0, 0],
    sensors: loadSensors(),
    sensorValues: [null, null, null, null],
    lastError: null,
  };

  constructor() {
    navigator.usb?.addEventListener('connect', (ev) => {
      if (ev.device.vendorId === LEGO_VENDOR_ID && ev.device.productId === NXT_PRODUCT_ID && this.state.status === 'disconnected') {
        void this.connectDevice(ev.device);
      }
    });
  }

  subscribe(fn: Listener): () => void {
    this.listeners.add(fn);
    fn(this.state);
    return () => this.listeners.delete(fn);
  }

  private update(patch: Partial<BrickState>) {
    this.state = { ...this.state, ...patch };
    for (const fn of this.listeners) fn(this.state);
  }

  get connected() {
    return this.state.status === 'connected';
  }

  /** Silently reconnects to a brick the user already granted access to. */
  async autoConnect(): Promise<boolean> {
    if (!WebUsbTransport.isSupported()) return false;
    const device = await WebUsbTransport.findPermitted().catch(() => null);
    if (!device) return false;
    await this.connectDevice(device);
    return this.connected;
  }

  async connectUsb(): Promise<void> {
    try {
      const device = await WebUsbTransport.request();
      await this.connectDevice(device);
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      this.update({
        status: 'disconnected',
        lastError: /No device selected|NotFoundError/i.test(message)
          ? 'No NXT found. Check the USB cable, switch the brick on, and make sure the WinUSB driver is installed.'
          : message,
      });
    }
  }

  async connectDemo(): Promise<void> {
    await this.attach(new DemoTransport());
  }

  private async connectDevice(device: USBDevice) {
    this.update({ status: 'connecting', lastError: null });
    try {
      await this.attach(await WebUsbTransport.open(device));
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      this.update({
        status: 'disconnected',
        lastError: /Access denied|SecurityError|claim/i.test(message)
          ? 'Windows would not let the app open the NXT. Install the WinUSB driver for it (see Brick → Driver setup).'
          : message,
      });
    }
  }

  private async attach(transport: Transport) {
    await this.disconnect();
    this.update({ status: 'connecting', lastError: null });
    this.transport = transport;
    transport.onDisconnect = () => void this.handleLinkLost('The NXT was disconnected.');
    this.failures = 0;
    this.sentPower = [null, null, null];
    try {
      const fw = P.parseFirmwareVersion(await this.request(P.getFirmwareVersion()));
      const info = P.parseDeviceInfo(await this.request(P.getDeviceInfo()));
      const battery = P.parseBatteryLevel(await this.request(P.getBatteryLevel()));
      this.lastBattery = this.lastKeepAlive = performance.now();
      this.update({
        status: 'connected',
        transport: transport.kind,
        name: info.name || 'NXT',
        firmware: fw.firmware,
        bluetoothAddress: info.bluetoothAddress,
        freeFlash: info.freeFlash,
        batteryMv: battery,
      });
      await this.applySensorModes();
      this.telemetryTimer = setInterval(() => void this.pollTelemetry(), TELEMETRY_INTERVAL_MS);
    } catch (err) {
      await this.handleLinkLost(err instanceof Error ? err.message : String(err));
    }
  }

  async disconnect(): Promise<void> {
    if (!this.transport) return;
    try {
      await this.stopAll();
    } catch {
      /* best effort */
    }
    await this.teardown();
    this.update({ status: 'disconnected', transport: null, batteryMv: null, sensorValues: [null, null, null, null] });
  }

  private async teardown() {
    if (this.telemetryTimer) clearInterval(this.telemetryTimer);
    this.telemetryTimer = null;
    const t = this.transport;
    this.transport = null;
    this.chain = Promise.resolve();
    if (t) await t.close().catch(() => {});
  }

  private async handleLinkLost(message: string) {
    await this.teardown();
    this.update({ status: 'disconnected', transport: null, batteryMv: null, lastError: message, sensorValues: [null, null, null, null] });
  }

  /** Runs `fn` after every previously queued command has finished. */
  private enqueue<T>(fn: (t: Transport) => Promise<T>): Promise<T> {
    const run = this.chain.then(() => {
      const t = this.transport;
      if (!t) throw new Error('Not connected to an NXT');
      return fn(t);
    });
    this.chain = run.catch(() => {});
    return run;
  }

  /** Sends a packet; resolves with the reply when one was requested, or an empty array. */
  request(packet: Uint8Array): Promise<Uint8Array> {
    return this.enqueue(async (t) => {
      await t.write(packet);
      if (!P.expectsReply(packet)) return new Uint8Array();
      // A reply left over from an earlier timed-out request may arrive first; skip it.
      for (let attempt = 0; attempt < 3; attempt++) {
        const reply = await t.read(REPLY_TIMEOUT_MS);
        if (reply[1] === packet[1]) return reply;
      }
      throw new Error('The NXT sent replies out of order');
    });
  }

  /** Sets target power for outputs A/B/C (-100..100). Updates are coalesced and only changes are sent. */
  setMotors(power: [number, number, number]) {
    const rounded = power.map((p) => Math.max(-100, Math.min(100, Math.round(p)))) as [number, number, number];
    this.desiredPower = rounded;
    if (rounded.some((p, i) => p !== this.state.motorPower[i])) this.update({ motorPower: rounded });
    this.scheduleMotorFlush();
  }

  private scheduleMotorFlush() {
    if (this.motorFlushQueued || !this.connected) return;
    this.motorFlushQueued = true;
    void this.enqueue(async (t) => {
      this.motorFlushQueued = false;
      const now = performance.now();
      const refresh = now - this.lastMotorSend > MOTOR_REFRESH_MS;
      for (let i = 0; i < 3; i++) {
        const p = this.desiredPower[i];
        if (!refresh && this.sentPower[i] === p) continue;
        await t.write(P.motorPower(i as 0 | 1 | 2, p, { brake: this.brakeOnStop, regulated: this.speedRegulation }));
        this.sentPower[i] = p;
      }
      if (refresh) this.lastMotorSend = now;
    }).catch((err) => this.noteFailure(err));
  }

  async stopAll(): Promise<void> {
    this.desiredPower = [0, 0, 0];
    this.update({ motorPower: [0, 0, 0] });
    await this.request(P.motorPower(P.PORT_ALL, 0, { brake: this.brakeOnStop }));
    this.sentPower = [0, 0, 0];
  }

  async playTone(hz: number, ms: number) {
    await this.request(P.playTone(hz, ms));
  }

  async resetRotation(port: 0 | 1 | 2) {
    await this.request(P.resetMotorPosition(port));
  }

  async setSensor(port: P.InputPort, kind: SensorKind) {
    const sensors = [...this.state.sensors];
    sensors[port] = kind;
    const sensorValues = [...this.state.sensorValues];
    sensorValues[port] = null;
    this.update({ sensors, sensorValues });
    try {
      localStorage.setItem(SENSOR_STORAGE_KEY, JSON.stringify(sensors));
    } catch {
      /* storage unavailable */
    }
    if (this.connected) {
      const k = SENSOR_KINDS[kind];
      await this.request(P.setInputMode(port, k.type, k.mode));
    }
  }

  private async applySensorModes() {
    for (let port = 0; port < 4; port++) {
      const k = SENSOR_KINDS[this.state.sensors[port]];
      await this.request(P.setInputMode(port as P.InputPort, k.type, k.mode));
    }
  }

  private noteFailure(err: unknown) {
    if (!this.transport) return;
    this.failures++;
    if (this.failures >= 3) void this.handleLinkLost(err instanceof Error ? err.message : String(err));
  }

  private async pollTelemetry() {
    if (this.telemetryBusy || !this.connected) return;
    this.telemetryBusy = true;
    try {
      const now = performance.now();
      const sensorValues = [...this.state.sensorValues];
      for (let port = 0; port < 4; port++) {
        if (this.state.sensors[port] === 'none') {
          sensorValues[port] = null;
          continue;
        }
        const v = P.parseInputValues(await this.request(P.getInputValues(port as P.InputPort)));
        sensorValues[port] = v.valid ? v.scaled : null;
      }
      const motorRotation = [0, 0, 0] as [number, number, number];
      for (let port = 0; port < 3; port++) {
        motorRotation[port] = P.parseOutputState(await this.request(P.getOutputState(port as 0 | 1 | 2))).rotationCount;
      }
      const patch: Partial<BrickState> = { sensorValues, motorRotation };
      if (now - this.lastBattery > BATTERY_INTERVAL_MS) {
        patch.batteryMv = P.parseBatteryLevel(await this.request(P.getBatteryLevel()));
        this.lastBattery = now;
      }
      if (now - this.lastKeepAlive > KEEPALIVE_INTERVAL_MS) {
        await this.request(P.keepAlive());
        this.lastKeepAlive = now;
      }
      this.failures = 0;
      if (this.connected) this.update(patch);
    } catch (err) {
      this.noteFailure(err);
    } finally {
      this.telemetryBusy = false;
    }
  }
}

export const brick = new NxtBrick();
