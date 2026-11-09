#include "BlockDefs.h"

#include <QCoreApplication>
#include <QHash>

namespace blocks {

namespace {

/** Translation context for block text (lupdate picks up Blocks::tr). */
struct Blocks {
    Q_DECLARE_TR_FUNCTIONS(Blocks)
};

ArgDef icon(const char *name) { return {ArgKind::Icon, {}, {}, {}, QStringLiteral(":/icons/%1.svg").arg(QLatin1String(name))}; }
ArgDef number(const char *name, const char *def) { return {ArgKind::Number, QLatin1String(name), {}, QLatin1String(def), {}}; }
ArgDef text(const char *name, const QString &def) { return {ArgKind::Text, QLatin1String(name), {}, def, {}}; }
ArgDef boolean(const char *name) { return {ArgKind::Bool, QLatin1String(name), {}, {}, {}}; }
ArgDef variable(const char *name) { return {ArgKind::Variable, QLatin1String(name), {}, {}, {}}; }
ArgDef dropdown(const char *name, QList<QPair<QString, QString>> options, const char *def = nullptr)
{
    const QString d = def ? QLatin1String(def) : options.value(0).second;
    return {ArgKind::Dropdown, QLatin1String(name), std::move(options), d, {}};
}

using Options = QList<QPair<QString, QString>>;

Options motorPorts() { return {{"A", "A"}, {"B", "B"}, {"C", "C"}}; }
Options motorPortsAll() { return motorPorts() << QPair<QString, QString>{"A+B+C", "ABC"}; }
Options sensorPorts() { return {{"1", "1"}, {"2", "2"}, {"3", "3"}, {"4", "4"}}; }
Options lessMore() { return {{"<", "LT"}, {">", "GT"}}; }
Options pressRelease() { return {{Blocks::tr("pressed"), "PRESSED"}, {Blocks::tr("released"), "RELEASED"}}; }
Options nxtButtons() { return {{Blocks::tr("orange"), "ENTER"}, {Blocks::tr("left"), "LEFT"}, {Blocks::tr("right"), "RIGHT"}}; }
Options messages()
{
    Options o;
    for (int i = 1; i <= 5; ++i) o << QPair<QString, QString>{Blocks::tr("message %1").arg(i), QString::number(i)};
    return o;
}
/** Values are drive::PadButton indexes. */
Options padButtons()
{
    return {{"A", "0"}, {"B", "1"}, {"X", "2"}, {"Y", "3"}, {"L1", "4"}, {"R1", "5"}, {"Select", "6"}, {"Start", "7"},
            {"L3", "8"}, {"R3", "9"}, {"↑", "10"}, {"↓", "11"}, {"←", "12"}, {"→", "13"}};
}
Options mailboxes()
{
    Options o;
    for (int i = 1; i <= 9; ++i) o << QPair<QString, QString>{QString::number(i), QString::number(i)};
    return o;
}
Options colours()
{
    return {{Blocks::tr("black"), "1"}, {Blocks::tr("blue"), "2"}, {Blocks::tr("green"), "3"},
            {Blocks::tr("yellow"), "4"}, {Blocks::tr("red"), "5"}, {Blocks::tr("white"), "6"}};
}
ArgDef units()
{
    return dropdown("UNIT", {{Blocks::tr("rotations"), "ROTATIONS"}, {Blocks::tr("degrees"), "DEGREES"}, {Blocks::tr("seconds"), "SECONDS"}});
}

RowDef row(const QString &message) { return {false, message, {}}; }
RowDef mouth(const char *name) { return {true, {}, QLatin1String(name)}; }

BlockDef def(const char *type, Category cat, Shape shape, QList<RowDef> rows, QList<ArgDef> args, QString tooltip = {})
{
    return {QLatin1String(type), cat, shape, std::move(rows), std::move(args), std::move(tooltip)};
}

QList<BlockDef> build()
{
    using C = Category;
    using S = Shape;
    QList<BlockDef> d;

    // ── Start ─────────────────────────────────────────
    d << def("nxt_when_start", C::Events, S::Hat, {row(Blocks::tr("%1 when program starts"))}, {icon("flag")},
             Blocks::tr("Runs the blocks below when the program starts on the NXT."));
    d << def("nxt_when_touch", C::Events, S::Hat, {row(Blocks::tr("%1 when touch %2 is %3"))},
             {icon("touch"), dropdown("PORT", sensorPorts()), dropdown("STATE", pressRelease())},
             Blocks::tr("Runs the blocks below every time the touch sensor changes like this."));
    d << def("nxt_when_button", C::Events, S::Hat, {row(Blocks::tr("%1 when NXT %2 button pressed"))},
             {icon("button"), dropdown("BUTTON", nxtButtons())});
    d << def("nxt_when_distance", C::Events, S::Hat, {row(Blocks::tr("%1 when distance %2 %3 %4 cm"))},
             {icon("ultrasonic"), dropdown("PORT", sensorPorts(), "4"), dropdown("OP", lessMore()), number("VALUE", "20")});
    d << def("nxt_when_light", C::Events, S::Hat, {row(Blocks::tr("%1 when light %2 %3 %4"))},
             {icon("light"), dropdown("PORT", sensorPorts(), "3"), dropdown("OP", lessMore()), number("VALUE", "50")});
    d << def("nxt_when_loud", C::Events, S::Hat, {row(Blocks::tr("%1 when sound level %2 > %3"))},
             {icon("sound"), dropdown("PORT", sensorPorts(), "2"), number("VALUE", "60")});
    d << def("nxt_when_timer", C::Events, S::Hat, {row(Blocks::tr("%1 when timer > %2 seconds"))}, {icon("timer"), number("SECONDS", "5")});
    d << def("nxt_when_message", C::Events, S::Hat, {row(Blocks::tr("%1 when I receive %2"))}, {icon("message"), dropdown("MESSAGE", messages())},
             Blocks::tr("Runs when another stack broadcasts this message."));
    d << def("nxt_broadcast", C::Events, S::Statement, {row(Blocks::tr("%1 broadcast %2"))}, {icon("message"), dropdown("MESSAGE", messages())},
             Blocks::tr("Starts every \"when I receive\" stack with the same message."));
    d << def("nxt_broadcast_wait", C::Events, S::Statement, {row(Blocks::tr("%1 broadcast %2 and wait"))},
             {icon("message"), dropdown("MESSAGE", messages())});

    // ── Motors ────────────────────────────────────────
    d << def("nxt_move_steer", C::Motors, S::Statement, {row(Blocks::tr("%1 move %2 + %3 steering %4 power %5 %"))},
             {icon("steer"), dropdown("LEFT", motorPorts(), "B"), dropdown("RIGHT", motorPorts(), "C"), number("STEER", "0"),
              number("POWER", "75")},
             Blocks::tr("Drives two motors together. Steering -100 spins left, 100 spins right, 0 goes straight."));
    d << def("nxt_motor_on", C::Motors, S::Statement, {row(Blocks::tr("%1 motor %2 on at %3 %"))},
             {icon("motor"), dropdown("PORT", motorPortsAll()), number("POWER", "75")},
             Blocks::tr("Turns a motor on and keeps it running. Negative power runs it backwards."));
    d << def("nxt_motor_for", C::Motors, S::Statement, {row(Blocks::tr("%1 motor %2 at %3 % for %4 %5"))},
             {icon("motor"), dropdown("PORT", motorPortsAll()), number("POWER", "75"), number("AMOUNT", "1"),
              dropdown("UNIT", {{Blocks::tr("rotations"), "ROTATIONS"}, {Blocks::tr("degrees"), "DEGREES"}, {Blocks::tr("seconds"), "SECONDS"}})},
             Blocks::tr("Runs a motor for a set amount, then stops it."));
    d << def("nxt_motor_stop", C::Motors, S::Statement, {row(Blocks::tr("%1 stop motor %2 and %3"))},
             {icon("stop"), dropdown("PORT", motorPortsAll(), "ABC"),
              dropdown("MODE", {{Blocks::tr("brake"), "BRAKE"}, {Blocks::tr("coast"), "COAST"}})},
             Blocks::tr("Brake holds the motor in place; coast lets it spin freely."));
    d << def("nxt_reset_rotation", C::Motors, S::Statement, {row(Blocks::tr("%1 reset rotation of motor %2"))},
             {icon("rotation"), dropdown("PORT", motorPorts())});
    d << def("nxt_motor_start_speed", C::Motors, S::Statement, {row(Blocks::tr("%1 set motor %2 speed to %3 %"))},
             {icon("motor"), dropdown("PORT", motorPortsAll()), number("POWER", "50")},
             Blocks::tr("Speed used by the next motor blocks that don't set their own."));
    d << def("nxt_motor_run_direction", C::Motors, S::Statement, {row(Blocks::tr("%1 run motor %2 %3 for %4 %5"))},
             {icon("motor"), dropdown("PORT", motorPortsAll()),
              dropdown("DIR", {{Blocks::tr("clockwise"), "CW"}, {Blocks::tr("counterclockwise"), "CCW"}}), number("AMOUNT", "1"), units()});
    d << def("nxt_motor_go_to", C::Motors, S::Statement, {row(Blocks::tr("%1 turn motor %2 to position %3 degrees"))},
             {icon("rotation"), dropdown("PORT", motorPorts()), number("DEGREES", "0")},
             Blocks::tr("Turns the motor the shortest way to an absolute position (0 = where it was last reset). "
                        "Within 2 degrees counts as already there, so it can run in a loop without twitching."));
    d << def("nxt_motor_power", C::Motors, S::Reporter, {row(Blocks::tr("%1 motor %2 power"))}, {icon("motor"), dropdown("PORT", motorPorts())});
    d << def("nxt_motor_running", C::Motors, S::Boolean, {row(Blocks::tr("%1 motor %2 running?"))}, {icon("motor"), dropdown("PORT", motorPorts())});

    // ── Movement (two-motor driving base) ─────────────
    d << def("nxt_move_for", C::Movement, S::Statement, {row(Blocks::tr("%1 move %2 for %3 %4"))},
             {icon("arrow"), dropdown("DIR", {{Blocks::tr("forward"), "FORWARD"}, {Blocks::tr("backward"), "BACKWARD"},
                                              {Blocks::tr("left", "direction"), "LEFT"}, {Blocks::tr("right", "direction"), "RIGHT"}}),
              number("AMOUNT", "2"), units()},
             Blocks::tr("Drives with the movement motors and speed set below, then stops."));
    d << def("nxt_move_start", C::Movement, S::Statement, {row(Blocks::tr("%1 start moving %2"))},
             {icon("arrow"), dropdown("DIR", {{Blocks::tr("forward"), "FORWARD"}, {Blocks::tr("backward"), "BACKWARD"},
                                              {Blocks::tr("left"), "LEFT"}, {Blocks::tr("right"), "RIGHT"}})});
    d << def("nxt_move_steer_for", C::Movement, S::Statement, {row(Blocks::tr("%1 move with steering %2 for %3 %4"))},
             {icon("steer"), number("STEER", "30"), number("AMOUNT", "1"), units()},
             Blocks::tr("Steering -100 spins left, 0 goes straight, 100 spins right."));
    d << def("nxt_move_steer_start", C::Movement, S::Statement, {row(Blocks::tr("%1 start moving with steering %2"))},
             {icon("steer"), number("STEER", "0")});
    d << def("nxt_move_tank_for", C::Movement, S::Statement, {row(Blocks::tr("%1 tank drive left %2 % right %3 % for %4 %5"))},
             {icon("tank"), number("LEFT", "50"), number("RIGHT", "50"), number("AMOUNT", "1"), units()});
    d << def("nxt_move_tank_start", C::Movement, S::Statement, {row(Blocks::tr("%1 start tank drive left %2 % right %3 %"))},
             {icon("tank"), number("LEFT", "50"), number("RIGHT", "50")});
    d << def("nxt_move_stop", C::Movement, S::Statement, {row(Blocks::tr("%1 stop moving"))}, {icon("stop")});
    d << def("nxt_move_set_speed", C::Movement, S::Statement, {row(Blocks::tr("%1 set movement speed to %2 %"))}, {icon("arrow"), number("POWER", "50")});
    d << def("nxt_move_set_motors", C::Movement, S::Statement, {row(Blocks::tr("%1 set movement motors to %2 + %3"))},
             {icon("steer"), dropdown("LEFT", motorPorts(), "B"), dropdown("RIGHT", motorPorts(), "C")});
    d << def("nxt_move_set_stop", C::Movement, S::Statement, {row(Blocks::tr("%1 when movement stops, %2"))},
             {icon("stop"), dropdown("MODE", {{Blocks::tr("brake"), "BRAKE"}, {Blocks::tr("coast"), "COAST"}})});

    // ── Sensors ───────────────────────────────────────
    d << def("nxt_touch_pressed", C::Sensors, S::Boolean, {row(Blocks::tr("%1 touch %2 pressed?"))},
             {icon("touch"), dropdown("PORT", sensorPorts())});
    d << def("nxt_light", C::Sensors, S::Reporter, {row(Blocks::tr("%1 light %2 %3"))},
             {icon("light"), dropdown("PORT", sensorPorts(), "3"),
              dropdown("MODE", {{Blocks::tr("reflected"), "REFLECTED"}, {Blocks::tr("ambient"), "AMBIENT"}})},
             Blocks::tr("Light level 0-100. \"Reflected\" turns on the sensor's red LED."));
    d << def("nxt_sound", C::Sensors, S::Reporter, {row(Blocks::tr("%1 sound level %2"))},
             {icon("sound"), dropdown("PORT", sensorPorts(), "2")});
    d << def("nxt_ultrasonic", C::Sensors, S::Reporter, {row(Blocks::tr("%1 distance cm %2"))},
             {icon("ultrasonic"), dropdown("PORT", sensorPorts(), "4")});
    d << def("nxt_rotation", C::Sensors, S::Reporter, {row(Blocks::tr("%1 motor %2 degrees"))},
             {icon("rotation"), dropdown("PORT", motorPorts())});
    d << def("nxt_button", C::Sensors, S::Boolean, {row(Blocks::tr("%1 NXT %2 button pressed?"))},
             {icon("button"), dropdown("BUTTON", {{Blocks::tr("orange"), "ENTER"}, {Blocks::tr("left"), "LEFT"}, {Blocks::tr("right"), "RIGHT"}})});
    d << def("nxt_timer", C::Sensors, S::Reporter, {row(Blocks::tr("%1 timer seconds"))}, {icon("timer")});
    d << def("nxt_reset_timer", C::Sensors, S::Statement, {row(Blocks::tr("%1 reset timer"))}, {icon("timer")});
    d << def("nxt_color", C::Sensors, S::Reporter, {row(Blocks::tr("%1 colour %2"))}, {icon("color"), dropdown("PORT", sensorPorts(), "3")},
             Blocks::tr("NXT 2.0 colour sensor: 1 black, 2 blue, 3 green, 4 yellow, 5 red, 6 white."));
    d << def("nxt_color_is", C::Sensors, S::Boolean, {row(Blocks::tr("%1 colour %2 is %3?"))},
             {icon("color"), dropdown("PORT", sensorPorts(), "3"), dropdown("COLOR", colours(), "5")});
    d << def("nxt_is_distance", C::Sensors, S::Boolean, {row(Blocks::tr("%1 distance %2 %3 %4 cm?"))},
             {icon("ultrasonic"), dropdown("PORT", sensorPorts(), "4"), dropdown("OP", lessMore()), number("VALUE", "20")});
    d << def("nxt_is_light", C::Sensors, S::Boolean, {row(Blocks::tr("%1 light %2 %3 %4?"))},
             {icon("light"), dropdown("PORT", sensorPorts(), "3"), dropdown("OP", lessMore()), number("VALUE", "50")});
    d << def("nxt_is_loud", C::Sensors, S::Boolean, {row(Blocks::tr("%1 sound level %2 > %3?"))},
             {icon("sound"), dropdown("PORT", sensorPorts(), "2"), number("VALUE", "60")});
    d << def("nxt_touch_count", C::Sensors, S::Reporter, {row(Blocks::tr("%1 touch %2 bump count"))}, {icon("touch"), dropdown("PORT", sensorPorts())},
             Blocks::tr("How many times the touch sensor was pressed and released since the program started."));
    d << def("nxt_battery", C::Sensors, S::Reporter, {row(Blocks::tr("%1 battery level mV"))}, {icon("battery")});
    d << def("nxt_gyro_rate", C::Sensors, S::Reporter, {row(Blocks::tr("%1 gyro %2 turn speed °/s"))}, {icon("gyro"), dropdown("PORT", sensorPorts(), "2")},
             Blocks::tr("HiTechnic gyro sensor: how fast the robot turns, in degrees per second (0 when still). "
                        "Turning one way gives positive numbers, the other way negative."));
    d << def("nxt_gyro_angle", C::Sensors, S::Reporter, {row(Blocks::tr("%1 gyro %2 angle °"))}, {icon("gyro"), dropdown("PORT", sensorPorts(), "2")},
             Blocks::tr("How many degrees the robot has turned since the program started or the angle was reset. "
                        "It slowly drifts, so reset it before each turn you measure."));
    d << def("nxt_gyro_reset", C::Sensors, S::Statement, {row(Blocks::tr("%1 reset gyro %2 angle"))}, {icon("gyro"), dropdown("PORT", sensorPorts(), "2")},
             Blocks::tr("Sets the gyro angle back to 0."));
    d << def("nxt_gyro_calibrate", C::Sensors, S::Statement, {row(Blocks::tr("%1 calibrate gyro %2 (keep still)"))},
             {icon("gyro"), dropdown("PORT", sensorPorts(), "2")},
             Blocks::tr("Measures the gyro's resting value for about a second and resets the angle. The robot must not move meanwhile. "
                        "This happens by itself when the program starts; use it again if the angle drifts a lot."));

    // ── Gamepad and Bluetooth ─────────────────────────
    d << def("pad_when_button", C::Gamepad, S::Hat, {row(Blocks::tr("%1 when gamepad button %2 is pressed"))},
             {icon("gamepad"), dropdown("BUTTON", padButtons())},
             Blocks::tr("The gamepad plugged into this PC. On the NXT it works while the app is connected (USB or Bluetooth)."));
    d << def("pad_button", C::Gamepad, S::Boolean, {row(Blocks::tr("%1 gamepad button %2 pressed?"))}, {icon("gamepad"), dropdown("BUTTON", padButtons())});
    d << def("pad_axis", C::Gamepad, S::Reporter, {row(Blocks::tr("%1 gamepad %2"))},
             {icon("gamepad"), dropdown("AXIS", {{Blocks::tr("left stick ↔"), "0"}, {Blocks::tr("left stick ↕"), "1"}, {Blocks::tr("right stick ↔"), "2"},
                                                 {Blocks::tr("right stick ↕"), "3"}, {Blocks::tr("trigger L2"), "4"}, {Blocks::tr("trigger R2"), "5"}})},
             Blocks::tr("Sticks give -100 to 100 (right and up are positive), triggers 0 to 100."));
    d << def("pad_drive", C::Gamepad, S::Statement, {row(Blocks::tr("%1 drive with the left stick: left motor %2 right motor %3"))},
             {icon("steer"), dropdown("LEFT", motorPorts(), "B"), dropdown("RIGHT", motorPorts(), "C")},
             Blocks::tr("Sets both motors from the left stick once. Put it inside \"forever\" to drive the robot with the gamepad."));
    d << def("nxt_mailbox_number", C::Gamepad, S::Reporter, {row(Blocks::tr("%1 number in mailbox %2"))}, {icon("bluetooth"), dropdown("BOX", mailboxes())},
             Blocks::tr("The last number another brick or the PC sent to this mailbox (0 until something arrives). Only on the NXT."));
    d << def("nxt_mailbox_send", C::Gamepad, S::Statement, {row(Blocks::tr("%1 send number %2 to mailbox %3 of brick %4"))},
             {icon("bluetooth"), number("VALUE", "1"), dropdown("BOX", mailboxes()), dropdown("CONN", {{"1", "1"}, {"2", "2"}, {"3", "3"}})},
             Blocks::tr("Sends over Bluetooth to a brick connected to this one (Bluetooth menu on the NXT, connection 1-3). Only on the NXT."));

    // ── Control ───────────────────────────────────────
    d << def("nxt_wait_seconds", C::Control, S::Statement, {row(Blocks::tr("%1 wait %2 seconds"))},
             {icon("clock"), number("SECONDS", "1")});
    d << def("nxt_wait_touch", C::Control, S::Statement, {row(Blocks::tr("%1 wait for touch %2 to be %3"))},
             {icon("touch"), dropdown("PORT", sensorPorts()),
              dropdown("STATE", {{Blocks::tr("pressed"), "PRESSED"}, {Blocks::tr("released"), "RELEASED"}, {Blocks::tr("bumped"), "BUMPED"}})});
    d << def("nxt_wait_until", C::Control, S::Statement, {row(Blocks::tr("%1 wait until %2"))}, {icon("clock"), boolean("CONDITION")});
    d << def("nxt_repeat", C::Control, S::Statement, {row(Blocks::tr("%1 repeat %2 times")), mouth("DO")},
             {icon("repeat"), number("TIMES", "4")});
    d << def("nxt_forever", C::Control, S::Cap, {row(Blocks::tr("%1 forever")), mouth("DO")}, {icon("repeat")});
    d << def("nxt_repeat_until", C::Control, S::Statement, {row(Blocks::tr("%1 repeat until %2")), mouth("DO")},
             {icon("repeat"), boolean("CONDITION")});
    d << def("nxt_if", C::Control, S::Statement, {row(Blocks::tr("if %1 then")), mouth("DO")}, {boolean("CONDITION")});
    d << def("nxt_if_else", C::Control, S::Statement, {row(Blocks::tr("if %1 then")), mouth("DO"), row(Blocks::tr("else")), mouth("ELSE")},
             {boolean("CONDITION")});
    d << def("nxt_stop_program", C::Control, S::Cap, {row(Blocks::tr("%1 stop program"))}, {icon("stop")});
    d << def("nxt_while", C::Control, S::Statement, {row(Blocks::tr("%1 while %2")), mouth("DO")}, {icon("repeat"), boolean("CONDITION")});
    d << def("nxt_wait_button", C::Control, S::Statement, {row(Blocks::tr("%1 wait for NXT %2 button"))},
             {icon("button"), dropdown("BUTTON", nxtButtons())});
    d << def("nxt_wait_distance", C::Control, S::Statement, {row(Blocks::tr("%1 wait until distance %2 %3 %4 cm"))},
             {icon("ultrasonic"), dropdown("PORT", sensorPorts(), "4"), dropdown("OP", lessMore()), number("VALUE", "20")});
    d << def("nxt_wait_color", C::Control, S::Statement, {row(Blocks::tr("%1 wait until colour %2 is %3"))},
             {icon("color"), dropdown("PORT", sensorPorts(), "3"), dropdown("COLOR", colours(), "5")});
    d << def("nxt_wait_gyro", C::Control, S::Statement, {row(Blocks::tr("%1 wait until gyro %2 angle %3 %4 °"))},
             {icon("gyro"), dropdown("PORT", sensorPorts(), "2"), dropdown("OP", lessMore(), "GT"), number("VALUE", "90")},
             Blocks::tr("For turning by an angle: reset the gyro angle, start turning, wait here, then stop."));
    d << def("nxt_run_parallel", C::Control, S::Statement, {row(Blocks::tr("%1 do both at once")), mouth("A"), row(Blocks::tr("and")), mouth("B")},
             {icon("tasks")}, Blocks::tr("Runs the two inner stacks at the same time and continues when both are done."));
    d << def("nxt_break", C::Control, S::Cap, {row(Blocks::tr("%1 exit loop"))}, {icon("stop")});
    d << def("nxt_stop_other", C::Control, S::Statement, {row(Blocks::tr("%1 stop other stacks"))}, {icon("stop")});

    // ── Sound ─────────────────────────────────────────
    d << def("nxt_play_tone", C::Sound, S::Statement, {row(Blocks::tr("%1 play tone %2 Hz for %3 ms"))},
             {icon("speaker"), number("FREQ", "440"), number("DURATION", "500")});
    d << def("nxt_play_note", C::Sound, S::Statement, {row(Blocks::tr("%1 play note %2 for %3 beats"))},
             {icon("speaker"),
              dropdown("NOTE", {{"C", "523"}, {"D", "587"}, {"E", "659"}, {"F", "698"}, {"G", "784"}, {"A", "880"}, {"H", "988"}, {"C'", "1047"}}),
              number("BEATS", "1")});
    d << def("nxt_play_note_octave", C::Sound, S::Statement, {row(Blocks::tr("%1 play note %2 octave %3 for %4 s"))},
             {icon("speaker"), dropdown("NOTE", {{"C", "0"}, {"C#", "1"}, {"D", "2"}, {"D#", "3"}, {"E", "4"}, {"F", "5"}, {"F#", "6"},
                                                 {"G", "7"}, {"G#", "8"}, {"A", "9"}, {"A#", "10"}, {"H", "11"}}),
              dropdown("OCTAVE", {{"3", "3"}, {"4", "4"}, {"5", "5"}, {"6", "6"}, {"7", "7"}}, "5"), number("SECONDS", "0.5")});
    d << def("nxt_beep", C::Sound, S::Statement, {row(Blocks::tr("%1 beep"))}, {icon("speaker")});
    d << def("nxt_play_file", C::Sound, S::Statement, {row(Blocks::tr("%1 play sound %2 %3"))},
             {icon("speaker"),
              dropdown("FILE", {{"! Startup", "! Startup.rso"}, {"! Click", "! Click.rso"}, {"! Attention", "! Attention.rso"},
                                {"Good Job", "Good Job.rso"}, {"Hello", "Hello.rso"}, {"Woops", "Woops.rso"}}),
              dropdown("WAIT", {{Blocks::tr("until done"), "WAIT"}, {Blocks::tr("and continue"), "NOWAIT"}})},
             Blocks::tr("Plays a sound file stored on the brick."));
    d << def("nxt_set_volume", C::Sound, S::Statement, {row(Blocks::tr("%1 set volume to %2 %"))}, {icon("speaker"), number("VOLUME", "75")});
    d << def("nxt_change_volume", C::Sound, S::Statement, {row(Blocks::tr("%1 change volume by %2"))}, {icon("speaker"), number("DELTA", "-10")});
    d << def("nxt_stop_sound", C::Sound, S::Statement, {row(Blocks::tr("%1 stop all sounds"))}, {icon("square")});

    // ── Display ───────────────────────────────────────
    Options lines;
    for (int i = 1; i <= 8; ++i) lines << QPair<QString, QString>{QString::number(i), QString::number(i)};
    d << def("nxt_display_text", C::Display, S::Statement, {row(Blocks::tr("%1 show %2 on line %3"))},
             {icon("screen"), text("TEXT", Blocks::tr("Hello!")), dropdown("LINE", lines)},
             Blocks::tr("Writes text or a number on the brick's screen (8 lines, 16 characters each)."));
    d << def("nxt_clear_screen", C::Display, S::Statement, {row(Blocks::tr("%1 clear screen"))}, {icon("screen")});
    d << def("nxt_display_at", C::Display, S::Statement, {row(Blocks::tr("%1 write %2 at x %3 y %4"))},
             {icon("pencil"), text("TEXT", Blocks::tr("Hi")), number("X", "0"), number("Y", "56")},
             Blocks::tr("Screen is 100 × 64 pixels; y = 56 is the top line."));
    d << def("nxt_display_number", C::Display, S::Statement, {row(Blocks::tr("%1 show number %2 on line %3"))},
             {icon("screen"), number("VALUE", "0"), dropdown("LINE", lines)});
    d << def("nxt_draw_line", C::Display, S::Statement, {row(Blocks::tr("%1 draw line from %2 , %3 to %4 , %5"))},
             {icon("pencil"), number("X1", "0"), number("Y1", "0"), number("X2", "99"), number("Y2", "63")});
    d << def("nxt_draw_circle", C::Display, S::Statement, {row(Blocks::tr("%1 draw circle at %2 , %3 radius %4"))},
             {icon("pencil"), number("X", "50"), number("Y", "32"), number("R", "10")});
    d << def("nxt_draw_rect", C::Display, S::Statement, {row(Blocks::tr("%1 draw rectangle at %2 , %3 size %4 × %5"))},
             {icon("pencil"), number("X", "10"), number("Y", "10"), number("W", "30"), number("H", "20")});
    d << def("nxt_draw_pixel", C::Display, S::Statement, {row(Blocks::tr("%1 draw dot at %2 , %3"))},
             {icon("pencil"), number("X", "50"), number("Y", "32")});
    d << def("nxt_display_face", C::Display, S::Statement, {row(Blocks::tr("%1 show face %2"))},
             {icon("screen"), dropdown("FACE", {{Blocks::tr("happy"), "HAPPY"}, {Blocks::tr("sad"), "SAD"}, {Blocks::tr("surprised"), "SURPRISED"},
                                                {Blocks::tr("sleepy"), "SLEEPY"}, {Blocks::tr("angry"), "ANGRY"}})},
             Blocks::tr("Draws a simple face on the whole screen."));

    // ── Logic ─────────────────────────────────────────
    d << def("logic_compare", C::Logic, S::Boolean, {row(QStringLiteral("%1 %2 %3"))},
             {number("A", "0"), dropdown("OP", {{"<", "LT"}, {"=", "EQ"}, {">", "GT"}, {"≤", "LTE"}, {"≥", "GTE"}, {"≠", "NEQ"}}),
              number("B", "50")});
    d << def("logic_and", C::Logic, S::Boolean, {row(Blocks::tr("%1 and %2"))}, {boolean("A"), boolean("B")});
    d << def("logic_or", C::Logic, S::Boolean, {row(Blocks::tr("%1 or %2"))}, {boolean("A"), boolean("B")});
    d << def("logic_not", C::Logic, S::Boolean, {row(Blocks::tr("not %1"))}, {boolean("A")});
    d << def("logic_boolean", C::Logic, S::Boolean, {row(QStringLiteral("%1"))},
             {dropdown("BOOL", {{Blocks::tr("true"), "TRUE"}, {Blocks::tr("false"), "FALSE"}})});

    // ── Math ──────────────────────────────────────────
    d << def("math_arithmetic", C::Math, S::Reporter, {row(QStringLiteral("%1 %2 %3"))},
             {number("A", "1"), dropdown("OP", {{"+", "ADD"}, {"−", "SUB"}, {"×", "MUL"}, {"÷", "DIV"}}), number("B", "1")});
    d << def("math_random", C::Math, S::Reporter, {row(Blocks::tr("random %1 to %2"))}, {number("FROM", "1"), number("TO", "100")});
    d << def("math_constrain", C::Math, S::Reporter, {row(Blocks::tr("limit %1 between %2 and %3"))},
             {number("VALUE", "50"), number("LOW", "-100"), number("HIGH", "100")});
    d << def("math_single", C::Math, S::Reporter, {row(QStringLiteral("%1 %2"))},
             {dropdown("OP", {{Blocks::tr("abs"), "ABS"}, {Blocks::tr("round"), "ROUND"}, {"√", "ROOT"}, {"−", "NEG"}, {Blocks::tr("floor"), "FLOOR"},
                              {Blocks::tr("ceiling"), "CEIL"}, {"sin", "SIN"}, {"cos", "COS"}, {"tan", "TAN"}}),
              number("NUM", "9")});
    d << def("math_modulo", C::Math, S::Reporter, {row(Blocks::tr("%1 mod %2"))}, {number("A", "10"), number("B", "3")});
    d << def("math_minmax", C::Math, S::Reporter, {row(QStringLiteral("%1 %2 %3"))},
             {dropdown("OP", {{Blocks::tr("min"), "MIN"}, {Blocks::tr("max"), "MAX"}}), number("A", "1"), number("B", "2")});
    d << def("math_between", C::Math, S::Boolean, {row(Blocks::tr("%1 is between %2 and %3?"))},
             {number("VALUE", "50"), number("LOW", "0"), number("HIGH", "100")});
    d << def("text_join", C::Math, S::Reporter, {row(Blocks::tr("join %1 %2"))}, {text("A", Blocks::tr("speed ")), text("B", QStringLiteral("50"))});
    d << def("text_length", C::Math, S::Reporter, {row(Blocks::tr("length of %1"))}, {text("TEXT", QStringLiteral("NXT"))});

    // ── Variables ─────────────────────────────────────
    d << def("data_variable", C::Variables, S::Reporter, {row(QStringLiteral("%1"))}, {variable("VAR")});
    d << def("data_set", C::Variables, S::Statement, {row(Blocks::tr("set %1 to %2"))}, {variable("VAR"), number("VALUE", "0")});
    d << def("data_change", C::Variables, S::Statement, {row(Blocks::tr("change %1 by %2"))}, {variable("VAR"), number("VALUE", "1")});
    d << def("data_show", C::Variables, S::Statement, {row(Blocks::tr("show %1 on line %2"))}, {variable("VAR"), dropdown("LINE", lines)},
             Blocks::tr("Shows the variable's name and value on the NXT screen."));

    return d;
}

} // namespace

QColor categoryColor(Category c)
{
    static const QColor colors[CategoryCount] = {
        QColor("#FFB020"), QColor("#3D8BFD"), QColor("#E8548F"), QColor("#12B5C9"), QColor("#4CAF7A"), QColor("#FF8A3D"),
        QColor("#9466FF"), QColor("#C95FE0"), QColor("#5B6CF0"), QColor("#2FB86C"), QColor("#FF6A55"),
    };
    return colors[int(c)];
}

QString categoryName(Category c)
{
    switch (c) {
    case Category::Events: return Blocks::tr("Start");
    case Category::Motors: return Blocks::tr("Motors");
    case Category::Movement: return Blocks::tr("Movement");
    case Category::Sensors: return Blocks::tr("Sensors");
    case Category::Gamepad: return Blocks::tr("Gamepad");
    case Category::Control: return Blocks::tr("Control");
    case Category::Sound: return Blocks::tr("Sound");
    case Category::Display: return Blocks::tr("Display");
    case Category::Logic: return Blocks::tr("Logic");
    case Category::Math: return Blocks::tr("Math");
    case Category::Variables: return Blocks::tr("Variables");
    }
    return {};
}

const ArgDef *BlockDef::arg(const QString &name) const
{
    for (const ArgDef &a : args)
        if (a.name == name) return &a;
    return nullptr;
}

const QList<BlockDef> &allDefs()
{
    static const QList<BlockDef> defs = build();
    return defs;
}

const BlockDef *findDef(const QString &type)
{
    static const QHash<QString, const BlockDef *> index = [] {
        QHash<QString, const BlockDef *> h;
        for (const BlockDef &d : allDefs()) h.insert(d.type, &d);
        return h;
    }();
    return index.value(type, nullptr);
}

QList<QString> paletteTypes(Category c)
{
    QList<QString> out;
    for (const BlockDef &d : allDefs())
        if (d.category == c && d.type != QLatin1String("data_variable")) out << d.type;
    return out;
}

QMap<QString, QString> defaultFields(const BlockDef &def)
{
    QMap<QString, QString> f;
    for (const ArgDef &a : def.args)
        if (a.kind != ArgKind::Icon) f.insert(a.name, a.defaultValue);
    return f;
}

} // namespace blocks
