import { NEUTRAL_INPUT, type ControlInput } from './mixer.ts';

/** Indices in the W3C "standard" gamepad mapping (XInput pads always use it). */
export const Button = {
  A: 0,
  B: 1,
  X: 2,
  Y: 3,
  LB: 4,
  RB: 5,
  LT: 6,
  RT: 7,
  Back: 8,
  Start: 9,
  LS: 10,
  RS: 11,
  Up: 12,
  Down: 13,
  Left: 14,
  Right: 15,
} as const;

export interface PadSnapshot {
  connected: boolean;
  id: string;
  index: number;
  standard: boolean;
  input: ControlInput;
  /** Analogue value per button 0..1. */
  buttons: number[];
  /** Buttons that went down since the previous poll. */
  pressed: Set<number>;
}

const DISCONNECTED: PadSnapshot = {
  connected: false,
  id: '',
  index: -1,
  standard: false,
  input: NEUTRAL_INPUT,
  buttons: [],
  pressed: new Set(),
};

type Listener = (s: PadSnapshot) => void;

/** Polls the Gamepad API at ~120 Hz and reports the active controller. */
export class GamepadPoller {
  private listeners = new Set<Listener>();
  private timer: ReturnType<typeof setInterval> | null = null;
  private prevButtons: boolean[] = [];
  /** Index of the controller to use; null picks the first connected one. */
  preferredIndex: number | null = null;
  snapshot: PadSnapshot = DISCONNECTED;

  start() {
    if (this.timer) return;
    this.timer = setInterval(() => this.poll(), 8);
  }

  stop() {
    if (this.timer) clearInterval(this.timer);
    this.timer = null;
  }

  subscribe(fn: Listener): () => void {
    this.listeners.add(fn);
    this.start();
    return () => this.listeners.delete(fn);
  }

  /** All currently connected controllers, for the picker. */
  list(): { index: number; id: string }[] {
    return Array.from(navigator.getGamepads?.() ?? [])
      .filter((g): g is Gamepad => !!g && g.connected)
      .map((g) => ({ index: g.index, id: g.id }));
  }

  private poll() {
    const pads = Array.from(navigator.getGamepads?.() ?? []).filter((g): g is Gamepad => !!g && g.connected);
    const pad = pads.find((g) => g.index === this.preferredIndex) ?? pads[0];
    if (!pad) {
      if (this.snapshot.connected) this.emit(DISCONNECTED);
      this.prevButtons = [];
      return;
    }
    const axis = (i: number) => pad.axes[i] ?? 0;
    const btn = (i: number) => pad.buttons[i]?.value ?? 0;
    const down = pad.buttons.map((b) => b.pressed);
    const pressed = new Set<number>();
    down.forEach((d, i) => d && !this.prevButtons[i] && pressed.add(i));
    this.prevButtons = down;

    const input: ControlInput = {
      leftX: axis(0),
      leftY: -axis(1),
      rightX: axis(2),
      rightY: -axis(3),
      leftTrigger: btn(Button.LT),
      rightTrigger: btn(Button.RT),
      leftBumper: down[Button.LB] ?? false,
      rightBumper: down[Button.RB] ?? false,
    };
    this.emit({
      connected: true,
      id: pad.id,
      index: pad.index,
      standard: pad.mapping === 'standard',
      input,
      buttons: pad.buttons.map((b) => b.value),
      pressed,
    });
  }

  private emit(s: PadSnapshot) {
    this.snapshot = s;
    for (const fn of this.listeners) fn(s);
  }
}

export const gamepad = new GamepadPoller();

/** Shortens "Xbox 360 Controller (XInput STANDARD GAMEPAD)" style ids for display. */
export function prettyPadName(id: string): string {
  const cleaned = id.replace(/\(.*?(Vendor|STANDARD GAMEPAD).*?\)/gi, '').replace(/\s+/g, ' ').trim();
  return cleaned || 'Game controller';
}
