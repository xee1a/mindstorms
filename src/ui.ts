// Small shared UI helpers.

let toastTimer: ReturnType<typeof setTimeout> | undefined;

export function toast(message: string, kind: 'info' | 'error' | 'success' = 'info') {
  const el = document.getElementById('toast');
  if (!el) return;
  el.textContent = message;
  el.className = `toast show ${kind}`;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), kind === 'error' ? 6000 : 2800);
}

/** Six AA cells: ~9.0 V fresh, the NXT starts warning around 6.5 V. */
export function batteryPercent(mv: number): number {
  return Math.round(Math.max(0, Math.min(1, (mv - 6500) / 2500)) * 100);
}

export function escapeHtml(s: string): string {
  return s.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]!);
}

/** SVG arc path for a gauge from -1..1, centred at the top of a circle. */
export function gaugeArc(cx: number, cy: number, r: number, value: number): string {
  const v = Math.max(-1, Math.min(1, value));
  if (Math.abs(v) < 0.005) return '';
  const start = -Math.PI / 2;
  const end = start + v * Math.PI * 0.75; // up to ±135°
  const p = (a: number) => `${(cx + r * Math.cos(a)).toFixed(2)} ${(cy + r * Math.sin(a)).toFixed(2)}`;
  const large = Math.abs(end - start) > Math.PI ? 1 : 0;
  return `M ${p(start)} A ${r} ${r} 0 ${large} ${v > 0 ? 1 : 0} ${p(end)}`;
}
