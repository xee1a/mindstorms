import { CommandType, DirectOp, SensorType, SystemOp } from './protocol.ts';
import type { Transport } from './transport.ts';

interface SimMotor {
  power: number;
  running: boolean;
  tacho: number;
}

/** A simulated brick that answers direct commands, for trying the app without hardware. */
export class DemoTransport implements Transport {
  readonly kind = 'demo';
  readonly label = 'Demo brick';
  onDisconnect?: () => void;

  private replies: Uint8Array[] = [];
  private motors: SimMotor[] = [0, 1, 2].map(() => ({ power: 0, running: false, tacho: 0 }));
  private sensorTypes = [0, 0, 0, 0];
  private lastTick = performance.now();
  private readonly started = performance.now();

  async write(packet: Uint8Array): Promise<void> {
    this.tick();
    const wantsReply = (packet[0] & CommandType.NoReply) === 0;
    const reply = this.handle(packet);
    if (wantsReply) this.replies.push(reply);
  }

  async read(): Promise<Uint8Array> {
    const r = this.replies.shift();
    if (!r) throw new Error('Demo brick: no reply queued');
    return r;
  }

  async close(): Promise<void> {}

  private tick() {
    const now = performance.now();
    const dt = (now - this.lastTick) / 1000;
    this.lastTick = now;
    // An NXT motor at full power turns at roughly 170 rpm, about 1000 deg/s.
    for (const m of this.motors) if (m.running) m.tacho += m.power * 10 * dt;
  }

  private ok(op: number, ...payload: number[]): Uint8Array {
    return Uint8Array.from([CommandType.Reply, op, 0, ...payload]);
  }

  private handle(p: Uint8Array): Uint8Array {
    const op = p[1];
    const t = (performance.now() - this.started) / 1000;
    switch (op) {
      case DirectOp.SetOutputState: {
        const ports = p[2] === 0xff ? [0, 1, 2] : [p[2]];
        for (const i of ports) {
          const m = this.motors[i];
          if (!m) continue;
          m.power = (p[3] << 24) >> 24;
          m.running = (p[4] & 0x01) !== 0 && p[7] !== 0;
        }
        return this.ok(op);
      }
      case DirectOp.SetInputMode:
        this.sensorTypes[p[2]] = p[3];
        return this.ok(op);
      case DirectOp.GetInputValues: {
        const port = p[2];
        const type = this.sensorTypes[port] ?? 0;
        const scaled = simulateSensor(type, t, port);
        const b = new Uint8Array(16);
        const v = new DataView(b.buffer);
        b.set([CommandType.Reply, op, 0, port, 1, 0, type, 0]);
        v.setUint16(8, scaled * 10, true);
        v.setUint16(10, scaled * 10, true);
        v.setInt16(12, scaled, true);
        return b;
      }
      case DirectOp.GetOutputState: {
        const m = this.motors[p[2]];
        const b = new Uint8Array(25);
        const v = new DataView(b.buffer);
        b.set([CommandType.Reply, op, 0, p[2], m.power & 0xff]);
        v.setInt32(13, Math.round(m.tacho), true);
        v.setInt32(21, Math.round(m.tacho), true);
        return b;
      }
      case DirectOp.ResetMotorPosition:
        this.motors[p[2]].tacho = 0;
        return this.ok(op);
      case DirectOp.GetBatteryLevel: {
        const mv = Math.round(7900 - (t % 600) * 0.5);
        return this.ok(op, mv & 0xff, mv >> 8);
      }
      case DirectOp.KeepAlive:
        return this.ok(op, 0x60, 0xea, 0, 0);
      case SystemOp.GetFirmwareVersion:
        return this.ok(op, 124, 1, 31, 1);
      case SystemOp.GetDeviceInfo: {
        const name = new TextEncoder().encode('DEMO-NXT');
        const b = new Uint8Array(33);
        b.set([CommandType.Reply, op, 0]);
        b.set(name, 3);
        b.set([0x00, 0x16, 0x53, 0x0a, 0xbc, 0xde], 18);
        new DataView(b.buffer).setUint32(29, 41_472, true);
        return b;
      }
      default:
        return this.ok(op);
    }
  }
}

function simulateSensor(type: number, t: number, port: number): number {
  switch (type) {
    case SensorType.Switch:
      return Math.sin(t * 1.3 + port) > 0.6 ? 1 : 0;
    case SensorType.LightActive:
    case SensorType.LightInactive:
      return Math.round(45 + 25 * Math.sin(t * 0.9 + port));
    case SensorType.SoundDb:
    case SensorType.SoundDba:
      return Math.round(Math.max(0, 20 + 30 * Math.sin(t * 2.1) * Math.sin(t * 0.37)));
    default:
      return 0;
  }
}
