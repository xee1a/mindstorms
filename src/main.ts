import './styles.css';
import { brick } from './nxt/brick.ts';
import { gamepad, prettyPadName } from './drive/gamepad.ts';
import { batteryPercent, toast } from './ui.ts';
import { mountProgram } from './views/program.ts';
import { mountDrive } from './views/drive.ts';
import { mountBrick } from './views/brick.ts';

type ViewName = 'program' | 'drive' | 'brick';

interface View {
  show?(): void;
  hide?(): void;
}

const $ = <T extends Element>(sel: string) => document.querySelector<T>(sel)!;

document.documentElement.classList.toggle('electron', navigator.userAgent.includes('Electron'));

const views: Record<ViewName, View> = {
  program: mountProgram($('#view-program')),
  drive: mountDrive($('#view-drive')),
  brick: mountBrick($('#view-brick')),
};

let current: ViewName | null = null;

function showView(name: ViewName) {
  if (name === current) return;
  if (current) views[current].hide?.();
  current = name;
  document.querySelectorAll<HTMLElement>('.view').forEach((el) => el.classList.toggle('active', el.id === `view-${name}`));
  document.querySelectorAll<HTMLButtonElement>('.tab').forEach((t) => {
    const on = t.dataset.view === name;
    t.classList.toggle('active', on);
    t.setAttribute('aria-selected', String(on));
  });
  views[name].show?.();
  try {
    localStorage.setItem('nxt-studio.view', name);
  } catch {
    /* storage unavailable */
  }
}

document.querySelectorAll<HTMLButtonElement>('.tab').forEach((t) =>
  t.addEventListener('click', () => showView(t.dataset.view as ViewName)),
);
$('#chip-brick').addEventListener('click', () => showView('brick'));

// Title bar status chips.
const padChip = $('#chip-pad');
gamepad.subscribe((s) => {
  padChip.classList.toggle('ok', s.connected);
  padChip.querySelector('.chip-label')!.textContent = s.connected ? prettyPadName(s.id) : 'No controller';
});

const brickChip = $('#chip-brick');
let lastError: string | null = null;
brick.subscribe((s) => {
  brickChip.classList.toggle('ok', s.status === 'connected');
  brickChip.classList.toggle('busy', s.status === 'connecting');
  const label =
    s.status === 'connected'
      ? `${s.name}${s.batteryMv !== null ? ` · ${batteryPercent(s.batteryMv)}%` : ''}`
      : s.status === 'connecting'
        ? 'Connecting…'
        : 'Not connected';
  brickChip.querySelector('.chip-label')!.textContent = label;
  if (s.lastError && s.lastError !== lastError) toast(s.lastError, 'error');
  lastError = s.lastError;
});

let initial: ViewName = 'program';
try {
  const saved = localStorage.getItem('nxt-studio.view');
  if (saved === 'program' || saved === 'drive' || saved === 'brick') initial = saved;
} catch {
  /* storage unavailable */
}
showView(initial);
void brick.autoConnect();
