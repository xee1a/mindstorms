// Original line icons for block faces, drawn white on a 24×24 grid.

const svg = (body: string) =>
  'data:image/svg+xml;utf8,' +
  encodeURIComponent(
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">${body}</svg>`,
  );

export const ICONS = {
  flag: svg('<path d="M5 21V4"/><path d="M5 4c3-2 6 2 9 0s4-1 5 0v9c-1-1-2-2-5 0s-6-2-9 0" fill="#fff" fill-opacity=".35"/>'),
  motor: svg(
    '<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="2.5" fill="#fff"/><path d="M12 4v3M12 17v3M4 12h3M17 12h3"/>',
  ),
  steer: svg('<path d="M4 18c4 0 8-3 8-8V4"/><path d="m9 7 3-3 3 3"/><path d="M20 18c-2 0-4-1-5.5-3"/>'),
  stop: svg('<rect x="6" y="6" width="12" height="12" rx="2.5" fill="#fff" fill-opacity=".35"/>'),
  touch: svg('<rect x="7" y="9" width="10" height="11" rx="2"/><path d="M12 9V4M9 4h6"/>'),
  light: svg('<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M5 5l1.5 1.5M17.5 17.5 19 19M5 19l1.5-1.5M17.5 6.5 19 5"/>'),
  sound: svg('<path d="M12 4a3 3 0 0 1 3 3v5a3 3 0 0 1-6 0V7a3 3 0 0 1 3-3z"/><path d="M6 11a6 6 0 0 0 12 0M12 17v3"/>'),
  ultrasonic: svg('<circle cx="7" cy="12" r="2.5"/><path d="M12 8a6 6 0 0 1 0 8M15.5 5a10 10 0 0 1 0 14"/>'),
  rotation: svg('<path d="M20 12a8 8 0 1 1-2.3-5.6"/><path d="M20 4v4h-4"/>'),
  button: svg('<path d="M12 3 21 12 12 21 3 12z"/><circle cx="12" cy="12" r="2" fill="#fff"/>'),
  clock: svg('<circle cx="12" cy="13" r="8"/><path d="M12 9v4l2.5 2.5M10 2h4"/>'),
  repeat: svg('<path d="M17 2l3 3-3 3"/><path d="M4 11V9a4 4 0 0 1 4-4h12"/><path d="M7 22l-3-3 3-3"/><path d="M20 13v2a4 4 0 0 1-4 4H4"/>'),
  speaker: svg('<path d="M4 9h4l5-4v14l-5-4H4z" fill="#fff" fill-opacity=".35"/><path d="M16.5 8.5a5 5 0 0 1 0 7M19 6a8.5 8.5 0 0 1 0 12"/>'),
  screen: svg('<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M7 9h6M7 12h10M9 21h6"/>'),
  timer: svg('<circle cx="12" cy="13" r="8"/><path d="M12 13V9M9 2h6M19 6l1.5-1.5"/>'),
};
