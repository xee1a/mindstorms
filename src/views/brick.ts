import { brick } from '../nxt/brick.ts';
import { WebUsbTransport } from '../nxt/transport.ts';
import { batteryPercent, escapeHtml, toast } from '../ui.ts';

export function mountBrick(root: HTMLElement) {
  const webUsb = WebUsbTransport.isSupported();
  root.innerHTML = `
    <div class="brick-grid">
      <article class="card brick-card">
        <div class="brick-visual" id="brick-visual">
          <div class="brick-body">
            <div class="brick-screen"><span id="screen-name">— — —</span><small id="screen-sub">offline</small></div>
            <div class="brick-buttons"><i></i><b></b><i></i></div>
            <div class="brick-grey"></div>
          </div>
        </div>
        <div class="brick-info">
          <h2 id="brick-title">No brick connected</h2>
          <p class="muted" id="brick-sub">Connect your NXT with its USB cable and switch it on.</p>
          <dl class="facts" id="facts"></dl>
          <div class="battery" id="battery" hidden>
            <div class="battery-shell"><span id="battery-fill"></span></div>
            <span id="battery-text"></span>
          </div>
          <div class="actions">
            <button class="btn primary" id="btn-connect"${webUsb ? '' : ' disabled'}>
              <svg viewBox="0 0 24 24"><path d="M12 3v8M8 7h8M7 11h10v3a5 5 0 0 1-10 0zM12 19v2"/></svg>Connect USB
            </button>
            <button class="btn ghost" id="btn-demo">
              <svg viewBox="0 0 24 24"><path d="M12 3l2.5 5.5L20 9l-4 4 1 6-5-3-5 3 1-6-4-4 5.5-.5z"/></svg>Try demo brick
            </button>
            <button class="btn ghost" id="btn-disconnect" hidden>Disconnect</button>
          </div>
          <div class="actions" id="quick" hidden>
            <button class="btn ghost" id="btn-beep">
              <svg viewBox="0 0 24 24"><path d="M4 9h4l5-4v14l-5-4H4z"/><path d="M16.5 8.5a5 5 0 0 1 0 7"/></svg>Beep
            </button>
            <button class="btn ghost" id="btn-stop">
              <svg viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/></svg>Stop motors
            </button>
          </div>
        </div>
      </article>

      <article class="card guide-card">
        <header class="card-head"><h2>Driver setup <span class="muted small">(one time)</span></h2></header>
        ${webUsb ? '' : '<p class="warn">This environment has no WebUSB. Run the desktop app, or use Chrome or Edge.</p>'}
        <p class="muted">NXT Studio talks to the brick through Windows' built-in <strong>WinUSB</strong> driver.
          The NXT doesn't install it automatically, so set it up once:</p>
        <ol class="steps">
          <li><span>1</span><div>Plug in the NXT and switch it on.</div></li>
          <li><span>2</span><div>Download <strong>Zadig</strong> from <code>zadig.akeo.ie</code> and run it.</div></li>
          <li><span>3</span><div>Choose <em>Options → List All Devices</em>, then pick <strong>LEGO MINDSTORMS NXT</strong>
            (USB ID <code>0694 0002</code>).</div></li>
          <li><span>4</span><div>Set the target driver to <strong>WinUSB</strong> and click <em>Install Driver</em>.</div></li>
          <li><span>5</span><div>Come back here and press <strong>Connect USB</strong>. After that the app reconnects automatically.</div></li>
        </ol>
        <details class="faq">
          <summary>Troubleshooting</summary>
          <ul>
            <li><strong>Nothing found:</strong> try a different cable or port, and make sure the brick is on (not just plugged in).</li>
            <li><strong>Access denied:</strong> another program (NXT-G, BricxCC) has the brick open, or the old Fantom driver is still bound. Close it and repeat the Zadig step.</li>
            <li><strong>Using old LEGO software too?</strong> NXT-G and BricxCC need the Fantom driver. Use Zadig or Device Manager to switch back when you need them.</li>
            <li><strong>Brick ticking/frozen:</strong> it's in firmware-update mode. Hold the reset button (in the Technic hole under the USB port) and reflash the LEGO firmware.</li>
          </ul>
        </details>
      </article>
    </div>`;

  const $ = <T extends HTMLElement = HTMLElement>(sel: string) => root.querySelector<T>(sel)!;

  $('#btn-connect').addEventListener('click', () => void brick.connectUsb());
  $('#btn-demo').addEventListener('click', async () => {
    await brick.connectDemo();
    if (brick.connected) toast('Connected to the demo brick. Motors and sensors are simulated.', 'success');
  });
  $('#btn-disconnect').addEventListener('click', () => void brick.disconnect());
  $('#btn-beep').addEventListener('click', () => brick.playTone(659, 150).then(() => brick.playTone(988, 200)).catch(() => {}));
  $('#btn-stop').addEventListener('click', () => brick.stopAll().catch(() => {}));

  brick.subscribe((s) => {
    const connected = s.status === 'connected';
    root.classList.toggle('connected', connected);
    $('#brick-visual').classList.toggle('on', connected);
    $('#screen-name').textContent = connected ? s.name : '— — —';
    $('#screen-sub').textContent = connected ? (s.transport === 'demo' ? 'demo' : 'usb') : s.status === 'connecting' ? 'linking…' : 'offline';
    $('#brick-title').textContent = connected ? s.name : s.status === 'connecting' ? 'Connecting…' : 'No brick connected';
    $('#brick-sub').textContent = connected
      ? s.transport === 'demo'
        ? 'Simulated brick. Everything works except real motors.'
        : 'Connected over USB.'
      : 'Connect your NXT with its USB cable and switch it on.';
    $('#facts').innerHTML = connected
      ? [
          ['Firmware', s.firmware],
          ['Free flash', `${(s.freeFlash / 1024).toFixed(1)} KB`],
          ['Bluetooth', s.bluetoothAddress],
        ]
          .map(([k, v]) => `<div><dt>${k}</dt><dd>${escapeHtml(v)}</dd></div>`)
          .join('')
      : '';
    const battery = $('#battery');
    battery.hidden = !connected || s.batteryMv === null;
    if (s.batteryMv !== null) {
      const pct = batteryPercent(s.batteryMv);
      $('#battery-fill').style.width = `${pct}%`;
      battery.classList.toggle('low', pct < 20);
      $('#battery-text').textContent = `${pct}% · ${(s.batteryMv / 1000).toFixed(2)} V`;
    }
    $('#btn-connect').hidden = connected;
    $('#btn-demo').hidden = connected;
    $('#btn-disconnect').hidden = !connected;
    $('#quick').hidden = !connected;
  });

  return {};
}
