import type * as Blockly from 'blockly';
import type { Category } from './theme.ts';

const n = (value: number) => ({ shadow: { type: 'math_number', fields: { NUM: value } } });
const block = (type: string, inputs?: Record<string, unknown>, fields?: Record<string, unknown>) => ({
  kind: 'block',
  type,
  ...(inputs ? { inputs } : {}),
  ...(fields ? { fields } : {}),
});

const category = (name: string, cat: Category, contents: unknown[], extra: Record<string, unknown> = {}) => ({
  kind: 'category',
  name,
  categorystyle: `${cat}_category`,
  cssConfig: { icon: `blocklyToolboxCategoryIcon cat-icon cat-${cat}` },
  contents,
  ...extra,
});

export const toolbox = {
  kind: 'categoryToolbox',
  contents: [
    category('Start', 'events', [block('nxt_when_start')]),
    category('Motors', 'motors', [
      block('nxt_move_steer', { STEER: n(0), POWER: n(75) }),
      block('nxt_motor_on', { POWER: n(75) }),
      block('nxt_motor_for', { POWER: n(75), AMOUNT: n(1) }),
      block('nxt_motor_stop'),
      block('nxt_reset_rotation'),
    ]),
    category('Sensors', 'sensors', [
      block('nxt_touch_pressed'),
      block('nxt_light'),
      block('nxt_sound', undefined, { PORT: '2' }),
      block('nxt_ultrasonic', undefined, { PORT: '4' }),
      block('nxt_rotation'),
      block('nxt_button'),
      block('nxt_timer'),
      block('nxt_reset_timer'),
    ]),
    category('Control', 'control', [
      block('nxt_wait_seconds', { SECONDS: n(1) }),
      block('nxt_wait_touch'),
      block('nxt_wait_until'),
      block('controls_repeat_ext', { TIMES: n(4) }),
      block('nxt_forever'),
      block('controls_whileUntil'),
      block('controls_if'),
      { ...block('controls_if'), extraState: { hasElse: true } },
      block('nxt_stop_program'),
    ]),
    category('Sound', 'sound', [
      block('nxt_play_tone', { FREQ: n(440), DURATION: n(500) }),
      block('nxt_play_note', { BEATS: n(1) }),
    ]),
    category('Display', 'display', [
      block('nxt_display_text', { TEXT: { shadow: { type: 'text', fields: { TEXT: 'Hello!' } } } }),
      block('nxt_clear_screen'),
    ]),
    { kind: 'sep', cssConfig: { container: 'cat-sep' } },
    category('Logic', 'logic', [
      block('logic_compare', { A: n(0), B: n(50) }),
      block('logic_operation'),
      block('logic_negate'),
      block('logic_boolean'),
    ]),
    category('Math', 'math', [
      block('math_number'),
      block('math_arithmetic', { A: n(1), B: n(1) }),
      block('math_random_int', { FROM: n(1), TO: n(100) }),
      block('math_constrain', { VALUE: n(50), LOW: n(-100), HIGH: n(100) }),
      block('math_single'),
      block('math_round'),
    ]),
    category('Variables', 'variables', [], { custom: 'VARIABLE' }),
  ],
} as unknown as Blockly.utils.toolbox.ToolboxDefinition;
