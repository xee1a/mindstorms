import { brick, SENSOR_KINDS, type SensorKind } from '../nxt/brick.ts';
import { OUTPUT_LABELS, type InputPort } from '../nxt/protocol.ts';
import { Button, gamepad, prettyPadName, type PadSnapshot } from '../drive/gamepad.ts';
import { DEFAULT_DRIVE_CONFIG, mix, NEUTRAL_INPUT, type ControlInput, type DriveConfig, type DriveMode } from '../drive/mixer.ts';
import { gaugeArc, toast } from '../ui.ts';

const CONFIG_KEY = 'nxt-studio.drive';
const TICK_MS = 20;

const MODES: Record<DriveMode, { label: string; hint: string }> = {
  arcade: { label: 'Arcade', hint: 'Left stick throttle · either stick X to steer' },
  tank: { label: 'Tank', hint: 'Left stick drives the left motor, right stick the right' },
  racing: { label: 'Racing', hint: 'RT forward · LT reverse · left stick steers' },
};

function loadConfig(): DriveConfig {
  try {
    return { ...DEFAULT_DRIVE_CONFIG, ...JSON.parse(localStorage.getItem(CONFIG_KEY) ?? '{}') };
  } catch {
    return { ...DEFAULT_DRIVE_CONFIG };
  }
}

const portOptions = (selected: number | null, allowNone = false) =>
  (allowNone ? [`<option value="none"${selected === null ? ' selected' : ''}>None</option>`] : [])
    .concat(OUTPUT_LABELS.map((l, i) => `<option value="${i}"${selected === i ? ' selected' : ''}>Motor ${l}</option>`))
    .join('');

const slider = (id: string, label: string, min: number, max: number, step: number, value: number, unit = '') => `
  <label class="field slider">
    <span class="field-label">${label}<output id="${id}-out">${value}${unit}</output></span>
    <input type="range" id="${id}" min="${min}" max="${max}" step="${step}" value="${value}" />
  </label>`;

const toggle = (id: string, label: string, checked: boolean) => `
  <label class="switch"><input type="checkbox" id="${id}"${checked ? ' checked' : ''} /><span class="track"><span class="thumb"></span></span>${label}</label>`;

const CONTROLLER_SVG = `
<svg class="pad-svg" viewBox="0 0 340 210" aria-hidden="true">
  <defs>
    <linearGradient id="padbody" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#253144"/><stop offset="1" stop-color="#171f2c"/>
    </linearGradient>
  </defs>
  <rect class="trig" id="pad-lt-bg" x="52" y="6" width="56" height="10" rx="5"/>
  <rect class="trig-fill" id="pad-lt" x="52" y="6" width="0" height="10" rx="5"/>
  <rect class="trig" id="pad-rt-bg" x="232" y="6" width="56" height="10" rx="5"/>
  <rect class="trig-fill" id="pad-rt" x="232" y="6" width="0" height="10" rx="5"/>
  <rect class="bumper" id="pad-lb" x="46" y="22" width="70" height="12" rx="6"/>
  <rect class="bumper" id="pad-rb" x="224" y="22" width="70" height="12" rx="6"/>
  <path class="pad-body" d="M70 40h200c32 0 52 22 58 58l10 62c4 26-14 44-36 44-16 0-26-9-34-22l-14-24H86l-14 24c-8 13-18 22-34 22-22 0-40-18-36-44l10-62c6-36 26-58 58-58z" fill="url(#padbody)"/>
  <circle class="well" cx="92" cy="92" r="27"/>
  <circle class="stick" id="pad-ls" cx="92" cy="92" r="14"/>
  <circle class="well" cx="214" cy="140" r="27"/>
  <circle class="stick" id="pad-rs" cx="214" cy="140" r="14"/>
  <g class="dpad">
    <rect id="pad-up" x="119" y="118" width="14" height="16" rx="3"/>
    <rect id="pad-down" x="119" y="146" width="14" height="16" rx="3"/>
    <rect id="pad-left" x="103" y="133" width="16" height="14" rx="3"/>
    <rect id="pad-right" x="133" y="133" width="16" height="14" rx="3"/>
  </g>
  <g class="face">
    <circle id="pad-y" cx="250" cy="70" r="11"/><text x="250" y="74">Y</text>
    <circle id="pad-x" cx="228" cy="92" r="11"/><text x="228" y="96">X</text>
    <circle id="pad-b" cx="272" cy="92" r="11"/><text x="272" y="96">B</text>
    <circle id="pad-a" cx="250" cy="114" r="11"/><text x="250" y="118">A</text>
  </g>
  <rect class="mini" id="pad-back" x="146" y="84" width="16" height="9" rx="4.5"/>
  <rect class="mini" id="pad-start" x="178" y="84" width="16" height="9" rx="4.5"/>
</svg>`;

export function mountDrive(root: HTMLElement) {
  const cfg = loadConfig();
  const saveConfig = () => {
    try {
      localStorage.setItem(CONFIG_KEY, JSON.stringify(cfg));
    } catch {
      /* storage unavailable */
    }
  };

  root.innerHTML = `
    <div class="drive-grid">
      <article class="card controller-card">
        <header class="card-head">
          <div>
            <h2>Controller</h2>
            <p class="muted small" id="pad-name">Plug in a USB controller and press any button</p>
          </div>
          <select id="pad-pick" class="select compact" hidden></select>
        </header>
        <div class="pad-stage">${CONTROLLER_SVG}</div>
        <div class="arm-row">
          <button class="arm" id="arm">
            <span class="arm-ring"></span>
            <span class="arm-text"><strong id="arm-title">Start driving</strong><small id="arm-sub">Start button or click</small></span>
          </button>
          <button class="btn danger" id="estop" title="Stop all motors (B button or Space)">
            <svg viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/></svg>Stop
          </button>
        </div>
        <ul class="hints">
          <li><kbd>A</kbd> beep</li>
          <li><kbd>B</kbd> / <kbd>Space</kbd> emergency stop</li>
          <li><kbd>LB</kbd> <kbd>RB</kbd> aux motor</li>
          <li><kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> drive without a controller</li>
        </ul>
      </article>

      <article class="card motors-card">
        <header class="card-head"><h2>Motors</h2><span class="muted small" id="link-note"></span></header>
        <div class="motors">
          ${OUTPUT_LABELS.map(
            (l, i) => `
            <div class="motor" data-port="${i}">
              <svg viewBox="0 0 120 120" class="gauge">
                <path class="gauge-track" d="${gaugeArc(60, 60, 48, -1)} ${gaugeArc(60, 60, 48, 1)}"/>
                <path class="gauge-value" id="gauge-${i}" d=""/>
                <text x="60" y="62" class="gauge-num" id="power-${i}">0</text>
                <text x="60" y="80" class="gauge-unit">power</text>
              </svg>
              <div class="motor-meta">
                <span class="port-badge">${l}</span>
                <span class="motor-role" id="role-${i}"></span>
              </div>
              <div class="motor-rot"><span id="rot-${i}">0°</span>
                <button class="icon-btn" data-reset="${i}" title="Reset rotation">
                  <svg viewBox="0 0 24 24"><path d="M4 12a8 8 0 1 0 2.3-5.6"/><path d="M4 4v4h4"/></svg>
                </button>
              </div>
            </div>`,
          ).join('')}
        </div>
        <div class="sensors">
          ${[0, 1, 2, 3]
            .map(
              (p) => `
            <div class="sensor" data-port="${p}">
              <div class="sensor-head"><span class="port-badge round">${p + 1}</span>
                <select class="select compact" data-sensor="${p}">
                  ${(Object.keys(SENSOR_KINDS) as SensorKind[]).map((k) => `<option value="${k}">${SENSOR_KINDS[k].label}</option>`).join('')}
                </select>
              </div>
              <div class="sensor-value" id="sv-${p}">–</div>
              <div class="bar"><span id="sb-${p}"></span></div>
            </div>`,
            )
            .join('')}
        </div>
      </article>

      <article class="card settings-card">
        <header class="card-head"><h2>Drive setup</h2></header>
        <div class="segmented" id="mode">
          ${(Object.keys(MODES) as DriveMode[]).map((m) => `<button data-mode="${m}">${MODES[m].label}</button>`).join('')}
        </div>
        <p class="muted small mode-hint" id="mode-hint"></p>
        <div class="field-grid">
          <label class="field"><span class="field-label">Left side</span><select class="select" id="left-motor">${portOptions(cfg.leftMotor)}</select></label>
          <label class="field"><span class="field-label">Right side</span><select class="select" id="right-motor">${portOptions(cfg.rightMotor)}</select></label>
          <label class="field"><span class="field-label">Aux (LB/RB)</span><select class="select" id="aux-motor">${portOptions(cfg.auxMotor, true)}</select></label>
        </div>
        <div class="toggles">
          ${toggle('inv-left', 'Reverse left', cfg.invertLeft)}
          ${toggle('inv-right', 'Reverse right', cfg.invertRight)}
          ${toggle('inv-aux', 'Reverse aux', cfg.invertAux)}
        </div>
        ${slider('max-power', 'Top speed', 10, 100, 5, cfg.maxPower, '%')}
        ${slider('aux-power', 'Aux power', 10, 100, 5, cfg.auxPower, '%')}
        ${slider('deadzone', 'Stick deadzone', 0, 0.4, 0.01, cfg.deadzone)}
        ${slider('expo', 'Fine control (expo)', 0, 1, 0.05, cfg.expo)}
        <div class="toggles">
          ${toggle('brake', 'Brake when stopped', brick.brakeOnStop)}
          ${toggle('regulate', 'Speed regulation', brick.speedRegulation)}
        </div>
      </article>
    </div>`;

  const $ = <T extends Element = HTMLElement>(sel: string) => root.querySelector<T>(sel)!;
  const sensorSelects = root.querySelectorAll<HTMLSelectElement>('[data-sensor]');

  // ── Settings wiring ───────────────────────────────────
  const renderMode = () => {
    root.querySelectorAll<HTMLButtonElement>('#mode button').forEach((b) => b.classList.toggle('active', b.dataset.mode === cfg.mode));
    $('#mode-hint').textContent = MODES[cfg.mode].hint;
  };
  const renderRoles = () => {
    for (let i = 0; i < 3; i++) {
      const roles = [];
      if (cfg.leftMotor === i) roles.push('Left drive');
      if (cfg.rightMotor === i && cfg.rightMotor !== cfg.leftMotor) roles.push('Right drive');
      if (cfg.auxMotor === i && i !== cfg.leftMotor && i !== cfg.rightMotor) roles.push('Aux');
      $(`#role-${i}`).textContent = roles.join(' · ') || 'Unused';
      root.querySelector(`.motor[data-port="${i}"]`)!.classList.toggle('unused', roles.length === 0);
    }
  };
  renderMode();
  renderRoles();

  $('#mode').addEventListener('click', (e) => {
    const m = (e.target as HTMLElement).closest<HTMLButtonElement>('button')?.dataset.mode as DriveMode | undefined;
    if (!m) return;
    cfg.mode = m;
    saveConfig();
    renderMode();
  });
  const bindSelect = (id: string, set: (v: string) => void) =>
    $<HTMLSelectElement>(id).addEventListener('change', (e) => {
      set((e.target as HTMLSelectElement).value);
      saveConfig();
      renderRoles();
    });
  bindSelect('#left-motor', (v) => (cfg.leftMotor = Number(v) as 0 | 1 | 2));
  bindSelect('#right-motor', (v) => (cfg.rightMotor = Number(v) as 0 | 1 | 2));
  bindSelect('#aux-motor', (v) => (cfg.auxMotor = v === 'none' ? null : (Number(v) as 0 | 1 | 2)));

  const bindToggle = (id: string, set: (v: boolean) => void) =>
    $<HTMLInputElement>(id).addEventListener('change', (e) => {
      set((e.target as HTMLInputElement).checked);
      saveConfig();
    });
  bindToggle('#inv-left', (v) => (cfg.invertLeft = v));
  bindToggle('#inv-right', (v) => (cfg.invertRight = v));
  bindToggle('#inv-aux', (v) => (cfg.invertAux = v));
  bindToggle('#brake', (v) => (brick.brakeOnStop = v));
  bindToggle('#regulate', (v) => (brick.speedRegulation = v));

  const bindSlider = (id: string, unit: string, set: (v: number) => void) => {
    const input = $<HTMLInputElement>(`#${id}`);
    const out = $<HTMLOutputElement>(`#${id}-out`);
    input.addEventListener('input', () => {
      set(Number(input.value));
      out.textContent = `${input.value}${unit}`;
      saveConfig();
    });
  };
  bindSlider('max-power', '%', (v) => (cfg.maxPower = v));
  bindSlider('aux-power', '%', (v) => (cfg.auxPower = v));
  bindSlider('deadzone', '', (v) => (cfg.deadzone = v));
  bindSlider('expo', '', (v) => (cfg.expo = v));

  sensorSelects.forEach((sel) =>
    sel.addEventListener('change', () => {
      brick.setSensor(Number(sel.dataset.sensor) as InputPort, sel.value as SensorKind).catch((err) => toast(String(err), 'error'));
    }),
  );
  root.querySelectorAll<HTMLButtonElement>('[data-reset]').forEach((b) =>
    b.addEventListener('click', () => {
      if (!brick.connected) return;
      brick.resetRotation(Number(b.dataset.reset) as 0 | 1 | 2).catch(() => {});
    }),
  );

  const padPick = $<HTMLSelectElement>('#pad-pick');
  padPick.addEventListener('change', () => (gamepad.preferredIndex = Number(padPick.value)));

  // ── Arming & safety ───────────────────────────────────
  let armed = false;
  const setArmed = (on: boolean) => {
    armed = on;
    root.classList.toggle('armed', on);
    $('#arm-title').textContent = on ? 'Driving' : 'Start driving';
    $('#arm-sub').textContent = on ? 'Start or B to stop' : 'Start button or click';
    if (!on) {
      brick.setMotors([0, 0, 0]);
    }
  };
  const emergencyStop = () => {
    setArmed(false);
    if (brick.connected) brick.stopAll().catch(() => {});
  };
  $('#arm').addEventListener('click', () => setArmed(!armed));
  $('#estop').addEventListener('click', emergencyStop);

  // Keyboard fallback (only used when no controller is connected).
  const keys = new Set<string>();
  const onKeyDown = (e: KeyboardEvent) => {
    if (e.target instanceof Element && e.target.closest('input, select, textarea')) return;
    if (e.code === 'Space') {
      e.preventDefault();
      emergencyStop();
      return;
    }
    keys.add(e.code);
  };
  const onKeyUp = (e: KeyboardEvent) => keys.delete(e.code);
  const keyboardInput = (): ControlInput => {
    const k = (...codes: string[]) => (codes.some((c) => keys.has(c)) ? 1 : 0);
    const y = k('KeyW', 'ArrowUp') - k('KeyS', 'ArrowDown');
    const x = k('KeyD', 'ArrowRight') - k('KeyA', 'ArrowLeft');
    return { ...NEUTRAL_INPUT, leftY: y, leftX: x * 0.7, rightY: y, leftBumper: keys.has('KeyQ'), rightBumper: keys.has('KeyE') };
  };

  const onPad = (s: PadSnapshot) => {
    if (!s.connected) {
      if (armed && keys.size === 0) {
        setArmed(false);
        toast('Controller disconnected, so motors were stopped.', 'error');
      }
      return;
    }
    if (s.pressed.has(Button.Start)) setArmed(!armed);
    if (s.pressed.has(Button.B)) emergencyStop();
    if (s.pressed.has(Button.A) && brick.connected) brick.playTone(880, 120).catch(() => {});
  };

  // ── Loop ──────────────────────────────────────────────
  let tickTimer: ReturnType<typeof setInterval> | null = null;
  let raf = 0;
  let unsubPad: (() => void) | null = null;
  let unsubBrick: (() => void) | null = null;

  const tick = () => {
    const pad = gamepad.snapshot;
    const input = pad.connected ? pad.input : keyboardInput();
    if (armed) brick.setMotors(mix(input, cfg));
  };

  const setAttr = (id: string, attr: string, v: string | number) => root.querySelector(`#${id}`)?.setAttribute(attr, String(v));
  const setPressed = (id: string, on: boolean) => root.querySelector(`#${id}`)?.classList.toggle('on', on);

  let lastPadId = '';
  const draw = () => {
    raf = requestAnimationFrame(draw);
    const pad = gamepad.snapshot;
    const input = pad.connected ? pad.input : keyboardInput();
    setAttr('pad-ls', 'cx', 92 + input.leftX * 13);
    setAttr('pad-ls', 'cy', 92 - input.leftY * 13);
    setAttr('pad-rs', 'cx', 214 + input.rightX * 13);
    setAttr('pad-rs', 'cy', 140 - input.rightY * 13);
    setAttr('pad-lt', 'width', 56 * input.leftTrigger);
    setAttr('pad-rt', 'width', 56 * input.rightTrigger);
    const b = pad.buttons;
    const on = (i: number) => (b[i] ?? 0) > 0.5;
    setPressed('pad-lb', input.leftBumper);
    setPressed('pad-rb', input.rightBumper);
    setPressed('pad-a', on(Button.A));
    setPressed('pad-b', on(Button.B));
    setPressed('pad-x', on(Button.X));
    setPressed('pad-y', on(Button.Y));
    setPressed('pad-up', on(Button.Up));
    setPressed('pad-down', on(Button.Down));
    setPressed('pad-left', on(Button.Left));
    setPressed('pad-right', on(Button.Right));
    setPressed('pad-back', on(Button.Back));
    setPressed('pad-start', on(Button.Start));
    root.querySelector('.pad-stage')!.classList.toggle('live', pad.connected || keys.size > 0);

    if (pad.id !== lastPadId) {
      lastPadId = pad.id;
      $('#pad-name').textContent = pad.connected
        ? `${prettyPadName(pad.id)}${pad.standard ? '' : ' · non-standard mapping'}`
        : 'Plug in a USB controller and press any button';
      const pads = gamepad.list();
      padPick.hidden = pads.length < 2;
      padPick.innerHTML = pads.map((p) => `<option value="${p.index}"${p.index === pad.index ? ' selected' : ''}>${prettyPadName(p.id)}</option>`).join('');
    }
  };

  const renderBrick = () =>
    brick.subscribe((s) => {
      for (let i = 0; i < 3; i++) {
        const p = s.motorPower[i];
        setAttr(`gauge-${i}`, 'd', gaugeArc(60, 60, 48, p / 100));
        root.querySelector(`#gauge-${i}`)!.classList.toggle('reverse', p < 0);
        $(`#power-${i}`).textContent = String(p);
        $(`#rot-${i}`).textContent = s.status === 'connected' ? `${s.motorRotation[i]}°` : '–';
      }
      $('#link-note').textContent =
        s.status === 'connected' ? (s.transport === 'demo' ? 'Demo brick (simulated)' : `Live on ${s.name}`) : 'Preview only, no brick connected';
      for (let p = 0; p < 4; p++) {
        const kind = s.sensors[p];
        sensorSelects[p].value = kind;
        const v = s.sensorValues[p];
        const el = $(`#sv-${p}`);
        const bar = $<HTMLElement>(`#sb-${p}`);
        root.querySelector(`.sensor[data-port="${p}"]`)!.classList.toggle('idle', kind === 'none');
        if (kind === 'none' || v === null) {
          el.textContent = '–';
          bar.style.width = '0%';
        } else if (kind === 'touch') {
          el.textContent = v ? 'Pressed' : 'Released';
          bar.style.width = v ? '100%' : '0%';
        } else {
          el.textContent = `${v}${SENSOR_KINDS[kind].unit ? ` ${SENSOR_KINDS[kind].unit}` : ''}`;
          bar.style.width = `${Math.max(0, Math.min(100, v))}%`;
        }
      }
    });

  return {
    show() {
      tickTimer = setInterval(tick, TICK_MS);
      raf = requestAnimationFrame(draw);
      unsubPad = gamepad.subscribe(onPad);
      unsubBrick = renderBrick();
      window.addEventListener('keydown', onKeyDown);
      window.addEventListener('keyup', onKeyUp);
      window.addEventListener('blur', onBlur);
    },
    hide() {
      setArmed(false);
      if (tickTimer) clearInterval(tickTimer);
      cancelAnimationFrame(raf);
      unsubPad?.();
      unsubBrick?.();
      keys.clear();
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
      window.removeEventListener('blur', onBlur);
    },
  };

  function onBlur() {
    // Released keys aren't reported while unfocused; drop them so the robot doesn't run away.
    keys.clear();
  }
}
