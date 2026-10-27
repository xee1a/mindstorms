import * as Blockly from 'blockly';

/** Category colours, shared with the toolbox and the app's CSS. */
export const PALETTE = {
  events: '#FFB020',
  motors: '#3D8BFD',
  sensors: '#12B5C9',
  control: '#FF8A3D',
  sound: '#A46BFF',
  display: '#EC5B9B',
  logic: '#5B6CF0',
  math: '#2FB86C',
  variables: '#FF6A55',
} as const;

export type Category = keyof typeof PALETTE;

function shade(hex: string, amount: number): string {
  const n = parseInt(hex.slice(1), 16);
  const ch = (shift: number) => {
    const c = (n >> shift) & 0xff;
    const v = amount < 0 ? c * (1 + amount) : c + (255 - c) * amount;
    return Math.round(v).toString(16).padStart(2, '0');
  };
  return `#${ch(16)}${ch(8)}${ch(0)}`;
}

const blockStyle = (hex: string, hat = '') => ({
  colourPrimary: hex,
  colourSecondary: shade(hex, 0.25),
  colourTertiary: shade(hex, -0.22),
  hat,
});

const blockStyles: Record<string, ReturnType<typeof blockStyle>> = {};
const categoryStyles: Record<string, { colour: string }> = {};
for (const [name, hex] of Object.entries(PALETTE)) {
  blockStyles[`${name}_blocks`] = blockStyle(hex, name === 'events' ? 'cap' : '');
  categoryStyles[`${name}_category`] = { colour: hex };
}
// Built-in blocks look up these style names.
blockStyles.logic_blocks = blockStyle(PALETTE.logic);
blockStyles.loop_blocks = blockStyle(PALETTE.control);
blockStyles.math_blocks = blockStyle(PALETTE.math);
blockStyles.text_blocks = blockStyle(PALETTE.display);
blockStyles.variable_blocks = blockStyle(PALETTE.variables);
blockStyles.variable_dynamic_blocks = blockStyle(PALETTE.variables);
blockStyles.procedure_blocks = blockStyle(PALETTE.variables);

export const studioTheme = Blockly.Theme.defineTheme('nxt-studio', {
  name: 'nxt-studio',
  base: Blockly.Themes.Classic,
  blockStyles,
  categoryStyles,
  componentStyles: {
    workspaceBackgroundColour: '#0f141c',
    toolboxBackgroundColour: '#131a24',
    toolboxForegroundColour: '#c9d4e3',
    flyoutBackgroundColour: '#18212e',
    flyoutForegroundColour: '#c9d4e3',
    flyoutOpacity: 0.97,
    scrollbarColour: '#3a4a60',
    scrollbarOpacity: 0.6,
    insertionMarkerColour: '#ffffff',
    insertionMarkerOpacity: 0.25,
    cursorColour: '#ffd166',
    selectedGlowColour: '#ffd166',
    selectedGlowOpacity: 0.9,
  },
  fontStyle: {
    family: '"Segoe UI Variable Text", "Segoe UI", system-ui, sans-serif',
    weight: '600',
    size: 11,
  },
  startHats: true,
});
