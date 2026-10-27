#pragma once

#include <QColor>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

namespace blocks {

enum class Category { Events, Motors, Sensors, Control, Sound, Display, Logic, Math, Variables };
constexpr int CategoryCount = 9;

QColor categoryColor(Category c);
QString categoryName(Category c);

enum class Shape {
    Hat,       // starts a script: no previous connection
    Statement, // stacks above and below
    Cap,       // stacks above only (forever, stop)
    Reporter,  // round value
    Boolean,   // hexagonal true/false value
};

enum class ArgKind {
    Icon,     // decorative image, `icon` is a resource path
    Dropdown, // fixed options
    Variable, // dropdown of the workspace's variables
    Number,   // round slot holding a number, accepts reporters
    Text,     // round slot holding text, accepts reporters and booleans
    Bool,     // hexagonal slot, accepts booleans
};

struct ArgDef {
    ArgKind kind = ArgKind::Icon;
    QString name;
    /** Dropdown options as (label, value). */
    QList<QPair<QString, QString>> options;
    QString defaultValue;
    QString icon;
};

/** One row of a block: inline text with %1-style arguments, or a C-shaped statement mouth. */
struct RowDef {
    bool statement = false;
    QString message; // inline rows
    QString name;    // statement rows
};

struct BlockDef {
    QString type;
    Category category = Category::Control;
    Shape shape = Shape::Statement;
    QList<RowDef> rows;
    /** Arguments in the order their %N placeholders are numbered, across all rows. */
    QList<ArgDef> args;
    QString tooltip;

    bool hasPrevious() const { return shape == Shape::Statement || shape == Shape::Cap; }
    bool hasNext() const { return shape == Shape::Statement || shape == Shape::Hat; }
    bool isValue() const { return shape == Shape::Reporter || shape == Shape::Boolean; }
    const ArgDef *arg(const QString &name) const;
};

/** All block types. Built on first use, so call after the translator is installed. */
const QList<BlockDef> &allDefs();
const BlockDef *findDef(const QString &type);

/** Blocks shown in the palette for a category (variable blocks are generated per variable). */
QList<QString> paletteTypes(Category c);

/** Field values a new block of this type starts with. */
QMap<QString, QString> defaultFields(const BlockDef &def);

} // namespace blocks
