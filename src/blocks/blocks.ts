import * as Blockly from 'blockly';
import { ICONS } from './icons.ts';

const icon = (src: string) => ({ type: 'field_image', src, width: 22, height: 22, alt: '' });

const MOTOR = [
  ['A', 'A'],
  ['B', 'B'],
  ['C', 'C'],
];
const MOTOR_OR_ALL = [...MOTOR, ['A + B + C', 'ABC']];
const SENSOR_PORT = [
  ['1', '1'],
  ['2', '2'],
  ['3', '3'],
  ['4', '4'],
];

const motorField = (name = 'PORT', options = MOTOR) => ({ type: 'field_dropdown', name, options });
const portField = (name = 'PORT') => ({ type: 'field_dropdown', name, options: SENSOR_PORT });
const num = (name: string, check = 'Number') => ({ type: 'input_value', name, check });

const stack = { previousStatement: null, nextStatement: null };

export const NXT_BLOCKS = [
  // ── Events ──────────────────────────────────────────────
  {
    type: 'nxt_when_start',
    message0: '%1 when program starts',
    args0: [icon(ICONS.flag)],
    nextStatement: null,
    style: 'events_blocks',
    tooltip: 'Runs the blocks below when the program is started on the NXT.',
  },

  // ── Motors ──────────────────────────────────────────────
  {
    type: 'nxt_motor_on',
    message0: '%1 motor %2 on at %3 %%',
    args0: [icon(ICONS.motor), motorField('PORT', MOTOR_OR_ALL), num('POWER')],
    inputsInline: true,
    ...stack,
    style: 'motors_blocks',
    tooltip: 'Turns a motor on and keeps it running. Power is -100 to 100; negative runs backwards.',
  },
  {
    type: 'nxt_motor_for',
    message0: '%1 motor %2 at %3 %% for %4 %5',
    args0: [
      icon(ICONS.motor),
      motorField('PORT', MOTOR_OR_ALL),
      num('POWER'),
      num('AMOUNT'),
      {
        type: 'field_dropdown',
        name: 'UNIT',
        options: [
          ['rotations', 'ROTATIONS'],
          ['degrees', 'DEGREES'],
          ['seconds', 'SECONDS'],
        ],
      },
    ],
    inputsInline: true,
    ...stack,
    style: 'motors_blocks',
    tooltip: 'Runs a motor for a set amount, then stops it.',
  },
  {
    type: 'nxt_move_steer',
    message0: '%1 move %2 + %3 steering %4 power %5 %%',
    args0: [icon(ICONS.steer), motorField('LEFT'), motorField('RIGHT'), num('STEER'), num('POWER')],
    inputsInline: true,
    ...stack,
    style: 'motors_blocks',
    tooltip: 'Drives two motors together. Steering -100 (spin left) to 100 (spin right), 0 goes straight.',
    extensions: ['nxt_default_right_motor'],
  },
  {
    type: 'nxt_motor_stop',
    message0: '%1 stop motor %2 and %3',
    args0: [
      icon(ICONS.stop),
      motorField('PORT', MOTOR_OR_ALL),
      {
        type: 'field_dropdown',
        name: 'MODE',
        options: [
          ['brake', 'BRAKE'],
          ['coast', 'COAST'],
        ],
      },
    ],
    ...stack,
    style: 'motors_blocks',
    tooltip: 'Stops a motor. Brake holds it in place; coast lets it spin freely.',
  },
  {
    type: 'nxt_reset_rotation',
    message0: '%1 reset rotation of motor %2',
    args0: [icon(ICONS.rotation), motorField()],
    ...stack,
    style: 'motors_blocks',
    tooltip: 'Sets the motor’s rotation counter back to 0.',
  },

  // ── Sensors ─────────────────────────────────────────────
  {
    type: 'nxt_touch_pressed',
    message0: '%1 touch %2 pressed?',
    args0: [icon(ICONS.touch), portField()],
    output: 'Boolean',
    style: 'sensors_blocks',
    tooltip: 'True while the touch sensor is pressed.',
  },
  {
    type: 'nxt_light',
    message0: '%1 light %2 %3',
    args0: [
      icon(ICONS.light),
      portField(),
      {
        type: 'field_dropdown',
        name: 'MODE',
        options: [
          ['reflected', 'REFLECTED'],
          ['ambient', 'AMBIENT'],
        ],
      },
    ],
    output: 'Number',
    style: 'sensors_blocks',
    tooltip: 'Light level 0-100. "Reflected" turns on the sensor’s red LED.',
  },
  {
    type: 'nxt_sound',
    message0: '%1 sound level %2',
    args0: [icon(ICONS.sound), portField()],
    output: 'Number',
    style: 'sensors_blocks',
    tooltip: 'Loudness 0-100.',
  },
  {
    type: 'nxt_ultrasonic',
    message0: '%1 distance cm %2',
    args0: [icon(ICONS.ultrasonic), portField()],
    output: 'Number',
    style: 'sensors_blocks',
    tooltip: 'Distance from the ultrasonic sensor in centimetres (0-255).',
  },
  {
    type: 'nxt_rotation',
    message0: '%1 motor %2 degrees',
    args0: [icon(ICONS.rotation), motorField()],
    output: 'Number',
    style: 'sensors_blocks',
    tooltip: 'How far a motor has turned, in degrees.',
  },
  {
    type: 'nxt_button',
    message0: '%1 NXT %2 button pressed?',
    args0: [
      icon(ICONS.button),
      {
        type: 'field_dropdown',
        name: 'BUTTON',
        options: [
          ['orange', 'ENTER'],
          ['left', 'LEFT'],
          ['right', 'RIGHT'],
        ],
      },
    ],
    output: 'Boolean',
    style: 'sensors_blocks',
    tooltip: 'True while that button on the brick is held down.',
  },
  {
    type: 'nxt_timer',
    message0: '%1 timer seconds',
    args0: [icon(ICONS.timer)],
    output: 'Number',
    style: 'sensors_blocks',
    tooltip: 'Seconds since the program started or the timer was reset.',
  },
  {
    type: 'nxt_reset_timer',
    message0: '%1 reset timer',
    args0: [icon(ICONS.timer)],
    ...stack,
    style: 'sensors_blocks',
  },

  // ── Control ─────────────────────────────────────────────
  {
    type: 'nxt_wait_seconds',
    message0: '%1 wait %2 seconds',
    args0: [icon(ICONS.clock), num('SECONDS')],
    inputsInline: true,
    ...stack,
    style: 'control_blocks',
  },
  {
    type: 'nxt_wait_until',
    message0: '%1 wait until %2',
    args0: [icon(ICONS.clock), { type: 'input_value', name: 'CONDITION', check: 'Boolean' }],
    ...stack,
    style: 'control_blocks',
    tooltip: 'Pauses until the condition becomes true.',
  },
  {
    type: 'nxt_wait_touch',
    message0: '%1 wait for touch %2 to be %3',
    args0: [
      icon(ICONS.touch),
      portField(),
      {
        type: 'field_dropdown',
        name: 'STATE',
        options: [
          ['pressed', 'PRESSED'],
          ['released', 'RELEASED'],
          ['bumped', 'BUMPED'],
        ],
      },
    ],
    ...stack,
    style: 'control_blocks',
  },
  {
    type: 'nxt_forever',
    message0: '%1 forever',
    args0: [icon(ICONS.repeat)],
    message1: '%1',
    args1: [{ type: 'input_statement', name: 'DO' }],
    previousStatement: null,
    style: 'control_blocks',
    tooltip: 'Repeats the blocks inside forever.',
  },
  {
    type: 'nxt_stop_program',
    message0: '%1 stop program',
    args0: [icon(ICONS.stop)],
    previousStatement: null,
    style: 'control_blocks',
  },

  // ── Sound ───────────────────────────────────────────────
  {
    type: 'nxt_play_tone',
    message0: '%1 play tone %2 Hz for %3 ms',
    args0: [icon(ICONS.speaker), num('FREQ'), num('DURATION')],
    inputsInline: true,
    ...stack,
    style: 'sound_blocks',
  },
  {
    type: 'nxt_play_note',
    message0: '%1 play note %2 for %3 beats',
    args0: [
      icon(ICONS.speaker),
      {
        type: 'field_dropdown',
        name: 'NOTE',
        options: [
          ['C', '523'],
          ['D', '587'],
          ['E', '659'],
          ['F', '698'],
          ['G', '784'],
          ['A', '880'],
          ['B', '988'],
          ['high C', '1047'],
        ],
      },
      num('BEATS'),
    ],
    inputsInline: true,
    ...stack,
    style: 'sound_blocks',
  },

  // ── Display ─────────────────────────────────────────────
  {
    type: 'nxt_display_text',
    message0: '%1 show %2 on line %3',
    args0: [
      icon(ICONS.screen),
      { type: 'input_value', name: 'TEXT' },
      { type: 'field_dropdown', name: 'LINE', options: [1, 2, 3, 4, 5, 6, 7, 8].map((n) => [String(n), String(n)]) },
    ],
    inputsInline: true,
    ...stack,
    style: 'display_blocks',
    tooltip: 'Writes text or a number on the brick’s screen (8 lines, 16 characters each).',
  },
  {
    type: 'nxt_clear_screen',
    message0: '%1 clear screen',
    args0: [icon(ICONS.screen)],
    ...stack,
    style: 'display_blocks',
  },
];

let registered = false;

export function registerBlocks() {
  if (registered) return;
  registered = true;
  // New steering blocks default to motors B + C, the usual drive pair.
  Blockly.Extensions.register('nxt_default_right_motor', function (this: Blockly.Block) {
    this.getField('LEFT')?.setValue('B');
    this.getField('RIGHT')?.setValue('C');
  });
  Blockly.common.defineBlocksWithJsonArray(NXT_BLOCKS);
}
