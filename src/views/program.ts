import * as Blockly from 'blockly';
import { registerBlocks } from '../blocks/blocks.ts';
import { studioTheme } from '../blocks/theme.ts';
import { toolbox } from '../blocks/toolbox.ts';
import { toast } from '../ui.ts';

const STORAGE_KEY = 'nxt-studio.workspace';
const NAME_KEY = 'nxt-studio.projectName';
const FILE_EXT = '.nxtb';

const STARTER = {
  blocks: {
    languageVersion: 0,
    blocks: [
      {
        type: 'nxt_when_start',
        x: 60,
        y: 60,
        next: {
          block: {
            type: 'nxt_move_steer',
            fields: { LEFT: 'B', RIGHT: 'C' },
            inputs: {
              STEER: { shadow: { type: 'math_number', fields: { NUM: 0 } } },
              POWER: { shadow: { type: 'math_number', fields: { NUM: 75 } } },
            },
            next: {
              block: {
                type: 'nxt_wait_touch',
                fields: { PORT: '1', STATE: 'PRESSED' },
                next: {
                  block: {
                    type: 'nxt_motor_stop',
                    fields: { PORT: 'ABC', MODE: 'BRAKE' },
                    next: {
                      block: {
                        type: 'nxt_play_tone',
                        inputs: {
                          FREQ: { shadow: { type: 'math_number', fields: { NUM: 880 } } },
                          DURATION: { shadow: { type: 'math_number', fields: { NUM: 200 } } },
                        },
                      },
                    },
                  },
                },
              },
            },
          },
        },
      },
    ],
  },
};

export function mountProgram(root: HTMLElement) {
  root.innerHTML = `
    <div class="toolbar">
      <div class="toolbar-group">
        <input class="project-name" id="project-name" spellcheck="false" aria-label="Project name" />
        <span class="muted small" id="block-count"></span>
      </div>
      <div class="toolbar-group">
        <button class="btn ghost" id="btn-new" title="New project">
          <svg viewBox="0 0 24 24"><path d="M12 5v14M5 12h14"/></svg>New
        </button>
        <button class="btn ghost" id="btn-open" title="Open a ${FILE_EXT} file">
          <svg viewBox="0 0 24 24"><path d="M4 7a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2z"/></svg>Open
        </button>
        <button class="btn ghost" id="btn-save" title="Save to a ${FILE_EXT} file">
          <svg viewBox="0 0 24 24"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg>Save
        </button>
        <span class="divider"></span>
        <button class="btn primary" id="btn-download" disabled title="Compiling and downloading to the NXT arrives in milestone 2">
          <svg viewBox="0 0 24 24"><path d="M7 4l12 8-12 8z"/></svg>Download &amp; run
          <span class="badge">soon</span>
        </button>
      </div>
      <input type="file" id="file-input" accept="${FILE_EXT},application/json" hidden />
    </div>
    <div class="blockly-host" id="blockly-host"></div>`;

  registerBlocks();
  const host = root.querySelector<HTMLElement>('#blockly-host')!;
  const nameInput = root.querySelector<HTMLInputElement>('#project-name')!;
  const countEl = root.querySelector<HTMLElement>('#block-count')!;
  const fileInput = root.querySelector<HTMLInputElement>('#file-input')!;

  const ws = Blockly.inject(host, {
    toolbox,
    theme: studioTheme,
    renderer: 'zelos',
    media: 'blockly-media/',
    grid: { spacing: 28, length: 2.5, colour: '#243246', snap: true },
    zoom: { controls: true, wheel: true, startScale: 0.9, maxScale: 2, minScale: 0.4, scaleSpeed: 1.15 },
    move: { scrollbars: true, drag: true, wheel: false },
    trashcan: true,
    sounds: false,
    maxInstances: { nxt_when_start: 1 },
  });

  const load = (state: unknown) => {
    Blockly.Events.disable();
    try {
      ws.clear();
      Blockly.serialization.workspaces.load(state as Record<string, unknown>, ws);
    } finally {
      Blockly.Events.enable();
    }
    updateCount();
  };

  const updateCount = () => {
    const n = ws.getAllBlocks(false).filter((b) => !b.isShadow()).length;
    countEl.textContent = `${n} block${n === 1 ? '' : 's'}`;
  };

  try {
    nameInput.value = localStorage.getItem(NAME_KEY) || 'My robot';
    const saved = localStorage.getItem(STORAGE_KEY);
    load(saved ? JSON.parse(saved) : STARTER);
  } catch {
    nameInput.value ||= 'My robot';
    load(STARTER);
  }

  let saveTimer: ReturnType<typeof setTimeout> | undefined;
  ws.addChangeListener((e) => {
    if (e.isUiEvent) return;
    updateCount();
    clearTimeout(saveTimer);
    saveTimer = setTimeout(() => {
      try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(Blockly.serialization.workspaces.save(ws)));
      } catch {
        /* storage unavailable */
      }
    }, 400);
  });

  nameInput.addEventListener('input', () => {
    try {
      localStorage.setItem(NAME_KEY, nameInput.value);
    } catch {
      /* storage unavailable */
    }
  });

  root.querySelector('#btn-new')!.addEventListener('click', () => {
    if (ws.getAllBlocks(false).length > 1 && !confirm('Start a new project? Unsaved blocks will be cleared.')) return;
    load(STARTER);
    nameInput.value = 'My robot';
    nameInput.dispatchEvent(new Event('input'));
  });

  root.querySelector('#btn-save')!.addEventListener('click', () => {
    const data = { format: 'nxt-studio', version: 1, name: nameInput.value, workspace: Blockly.serialization.workspaces.save(ws) };
    const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }));
    const a = document.createElement('a');
    a.href = url;
    a.download = `${(nameInput.value || 'program').replace(/[\\/:*?"<>|]+/g, '_')}${FILE_EXT}`;
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  });

  root.querySelector('#btn-open')!.addEventListener('click', () => fileInput.click());
  fileInput.addEventListener('change', async () => {
    const file = fileInput.files?.[0];
    fileInput.value = '';
    if (!file) return;
    try {
      const data = JSON.parse(await file.text());
      if (data?.format !== 'nxt-studio' || !data.workspace) throw new Error('not an NXT Studio project');
      load(data.workspace);
      nameInput.value = data.name || file.name.replace(/\.[^.]+$/, '');
      nameInput.dispatchEvent(new Event('input'));
      toast(`Opened “${nameInput.value}”`, 'success');
    } catch (err) {
      toast(`Couldn't open ${file.name}: ${err instanceof Error ? err.message : err}`, 'error');
    }
  });

  return {
    show() {
      // Blockly measures its container, which has no size while hidden.
      requestAnimationFrame(() => Blockly.svgResize(ws));
    },
  };
}
