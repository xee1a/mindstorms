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

    // ── Sound ─────────────────────────────────────────
    d << def("nxt_play_tone", C::Sound, S::Statement, {row(Blocks::tr("%1 play tone %2 Hz for %3 ms"))},
             {icon("speaker"), number("FREQ", "440"), number("DURATION", "500")});
    d << def("nxt_play_note", C::Sound, S::Statement, {row(Blocks::tr("%1 play note %2 for %3 beats"))},
             {icon("speaker"),
              dropdown("NOTE", {{"C", "523"}, {"D", "587"}, {"E", "659"}, {"F", "698"}, {"G", "784"}, {"A", "880"}, {"H", "988"}, {"C'", "1047"}}),
              number("BEATS", "1")});

    // ── Display ───────────────────────────────────────
    Options lines;
    for (int i = 1; i <= 8; ++i) lines << QPair<QString, QString>{QString::number(i), QString::number(i)};
    d << def("nxt_display_text", C::Display, S::Statement, {row(Blocks::tr("%1 show %2 on line %3"))},
             {icon("screen"), text("TEXT", Blocks::tr("Hello!")), dropdown("LINE", lines)},
             Blocks::tr("Writes text or a number on the brick's screen (8 lines, 16 characters each)."));
    d << def("nxt_clear_screen", C::Display, S::Statement, {row(Blocks::tr("%1 clear screen"))}, {icon("screen")});

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
             {dropdown("OP", {{Blocks::tr("abs"), "ABS"}, {Blocks::tr("round"), "ROUND"}, {"√", "ROOT"}, {"−", "NEG"}}), number("NUM", "9")});

    // ── Variables ─────────────────────────────────────
    d << def("data_variable", C::Variables, S::Reporter, {row(QStringLiteral("%1"))}, {variable("VAR")});
    d << def("data_set", C::Variables, S::Statement, {row(Blocks::tr("set %1 to %2"))}, {variable("VAR"), number("VALUE", "0")});
    d << def("data_change", C::Variables, S::Statement, {row(Blocks::tr("change %1 by %2"))}, {variable("VAR"), number("VALUE", "1")});

    return d;
}

} // namespace

QColor categoryColor(Category c)
{
    static const QColor colors[CategoryCount] = {
        QColor("#FFB020"), QColor("#3D8BFD"), QColor("#12B5C9"), QColor("#FF8A3D"), QColor("#A46BFF"),
        QColor("#EC5B9B"), QColor("#5B6CF0"), QColor("#2FB86C"), QColor("#FF6A55"),
    };
    return colors[int(c)];
}

QString categoryName(Category c)
{
    switch (c) {
    case Category::Events: return Blocks::tr("Start");
    case Category::Motors: return Blocks::tr("Motors");
    case Category::Sensors: return Blocks::tr("Sensors");
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
