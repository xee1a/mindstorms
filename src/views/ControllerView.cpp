#include "ControllerView.h"

#include "app/Prefs.h"
#include "app/Theme.h"
#include "app/Widgets.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <cmath>

using drive::PadButton;
using drive::PadSnapshot;

namespace {

constexpr int TickMs = 20;
// Below this a stick counts as centred (cheap pads rest a little off zero).
constexpr double StickRest = 0.08;

int percent(double v) { return int(std::lround(v * 100)); }

/** The pad's own name for an input, when it differs from the block name. */
QString padName(const QString &blockName, ui::PadView::Layout layout)
{
    using L = ui::PadView::Layout;
    static const QHash<QString, QString> playStation{{"A", "✕"}, {"B", "○"}, {"X", "□"}, {"Y", "△"}};
    static const QHash<QString, QString> xbox{{"L1", "LB"}, {"R1", "RB"}, {"L2", "LT"}, {"R2", "RT"}, {"Select", "View"}, {"Start", "Menu"}};
    if (layout == L::PlayStation) return playStation.value(blockName);
    if (layout == L::Xbox) return xbox.value(blockName);
    return {};
}

} // namespace

ControllerView::ControllerView(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 14);

    auto *card = ui::card(this);
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(10);
    lay->addWidget(card);

    // ── Header: which pad, and whether it's in a usable mode ──
    auto *head = new QHBoxLayout;
    head->setSpacing(10);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::label(tr("Controller"), "h1", card));
    m_padName = ui::label({}, "muted", card);
    m_padName->setWordWrap(true);
    titles->addWidget(m_padName);
    head->addLayout(titles, 1);
    m_padPick = new QComboBox(card);
    m_padPick->setProperty("compact", true);
    m_padPick->hide();
    connect(m_padPick, &QComboBox::activated, this, [this] { drive::Gamepad::instance().setPreferredSlot(m_padPick->currentData().toInt()); });
    head->addWidget(m_padPick, 0, Qt::AlignVCenter);
    // Only changes the drawing and the button names shown; the blocks always use the names in the lists.
    m_layoutPick = new QComboBox(card);
    m_layoutPick->setToolTip(tr("How the pad is drawn"));
    m_layoutPick->addItem(QStringLiteral("MANGAN 200"), int(ui::PadView::Layout::Mangan200));
    m_layoutPick->addItem(tr("Generic PlayStation layout"), int(ui::PadView::Layout::PlayStation));
    m_layoutPick->addItem(tr("Generic Xbox layout"), int(ui::PadView::Layout::Xbox));
    m_layoutPick->setCurrentIndex(std::max(0, m_layoutPick->findData(Prefs::instance().padLayout())));
    connect(m_layoutPick, &QComboBox::activated, this, [this] {
        Prefs::instance().setPadLayout(m_layoutPick->currentData().toInt());
        applyLayout();
    });
    head->addWidget(m_layoutPick, 0, Qt::AlignVCenter);
    v->addLayout(head);

    m_digitalHint = ui::label(tr("<b>The pad is in digital mode.</b> In this mode the pad itself sends the left stick as the arrows "
                                 "and the right stick as the A, B, X and Y buttons, so a program can't tell them apart. "
                                 "Press <b>ANALOG</b> (or <b>MODE</b>) on the pad so its light comes on."),
                              "warn", card);
    m_digitalHint->setWordWrap(true);
    m_digitalHint->hide();
    v->addWidget(m_digitalHint);

    // ── Inputs | drawing | inputs ─────────────────────────
    auto *middle = new QHBoxLayout;
    middle->setSpacing(18);
    auto *leftBox = new QWidget(card);
    auto *left = new QGridLayout(leftBox);
    auto *rightBox = new QWidget(card);
    auto *right = new QGridLayout(rightBox);
    for (QGridLayout *g : {left, right}) {
        g->setContentsMargins(0, 0, 0, 0);
        g->setHorizontalSpacing(10);
        g->setVerticalSpacing(9);
    }
    m_pad = new ui::PadView(card);
    middle->addStretch(1);
    middle->addWidget(leftBox, 0, Qt::AlignVCenter);
    middle->addWidget(m_pad, 4);
    middle->addWidget(rightBox, 0, Qt::AlignVCenter);
    middle->addStretch(1);
    v->addLayout(middle, 1);

    const auto pressed = [](PadButton b) { return [b](const PadSnapshot &s) { return s.pressed(b); }; };
    const auto state = [](PadButton b) { return [b](const PadSnapshot &s) { return s.pressed(b) ? tr("pressed") : tr("released"); }; };
    const auto trigger = [](bool rightSide) {
        return [rightSide](const PadSnapshot &s) { return QString::number(percent(rightSide ? s.input.rightTrigger : s.input.leftTrigger)); };
    };
    const auto stick = [](bool rightSide) {
        return [rightSide](const PadSnapshot &s) {
            const double x = rightSide ? s.input.rightX : s.input.leftX, y = rightSide ? s.input.rightY : s.input.leftY;
            return QStringLiteral("↔ %1   ↕ %2").arg(percent(x)).arg(percent(y));
        };
    };
    const auto moved = [](bool rightSide) {
        return [rightSide](const PadSnapshot &s) {
            return std::hypot(rightSide ? s.input.rightX : s.input.leftX, rightSide ? s.input.rightY : s.input.leftY) > StickRest;
        };
    };

    // Names match the Gamepad blocks' menus.
    addInput(left, true, QStringLiteral("L2"), [](const PadSnapshot &s) { return s.input.leftTrigger > 0.05; }, trigger(false));
    addInput(left, true, QStringLiteral("L1"), pressed(PadButton::LB), state(PadButton::LB));
    addInput(left, true, tr("Left stick"), moved(false), stick(false));
    addInput(left, true, QStringLiteral("L3"), pressed(PadButton::LS), state(PadButton::LS));
    addInput(left, true, QStringLiteral("↑"), pressed(PadButton::Up), state(PadButton::Up));
    addInput(left, true, QStringLiteral("↓"), pressed(PadButton::Down), state(PadButton::Down));
    addInput(left, true, QStringLiteral("←"), pressed(PadButton::Left), state(PadButton::Left));
    addInput(left, true, QStringLiteral("→"), pressed(PadButton::Right), state(PadButton::Right));
    addInput(left, true, QStringLiteral("Select"), pressed(PadButton::Back), state(PadButton::Back));

    addInput(right, false, QStringLiteral("R2"), [](const PadSnapshot &s) { return s.input.rightTrigger > 0.05; }, trigger(true));
    addInput(right, false, QStringLiteral("R1"), pressed(PadButton::RB), state(PadButton::RB));
    addInput(right, false, tr("Right stick"), moved(true), stick(true));
    addInput(right, false, QStringLiteral("R3"), pressed(PadButton::RS), state(PadButton::RS));
    addInput(right, false, QStringLiteral("Y"), pressed(PadButton::Y), state(PadButton::Y));
    addInput(right, false, QStringLiteral("X"), pressed(PadButton::X), state(PadButton::X));
    addInput(right, false, QStringLiteral("B"), pressed(PadButton::B), state(PadButton::B));
    addInput(right, false, QStringLiteral("A"), pressed(PadButton::A), state(PadButton::A));
    addInput(right, false, QStringLiteral("Start"), pressed(PadButton::Start), state(PadButton::Start));

    auto *foot = ui::label(tr("This tab only shows what the pad sends. To make the robot react, use the blocks in the "
                              "<b>Gamepad</b> category on the Program tab; the names here match the ones in the blocks. "
                              "Sticks give -100 to 100 (right and up are positive), triggers 0 to 100."),
                           "faint", card);
    foot->setWordWrap(true);
    foot->setAlignment(Qt::AlignHCenter);
    v->addWidget(foot);

    m_tick.setInterval(TickMs);
    connect(&m_tick, &QTimer::timeout, this, &ControllerView::tick);
    auto &pad = drive::Gamepad::instance();
    connect(&pad, &drive::Gamepad::connectionChanged, this, &ControllerView::onPadConnection);
    onPadConnection(pad.snapshot().connected);
    applyLayout();
    tick();
}

void ControllerView::applyLayout()
{
    const auto layout = ui::PadView::Layout(m_layoutPick->currentData().toInt());
    m_pad->setLayout(layout);
    for (InputRow &r : m_rows) {
        const QString own = padName(r.name, layout);
        r.chip->setText(own.isEmpty() ? r.name : QStringLiteral("%1  ·  %2").arg(r.name, own));
    }
}

void ControllerView::addInput(QGridLayout *grid, bool chipOnRight, const QString &name, std::function<bool(const PadSnapshot &)> active,
                              std::function<QString(const PadSnapshot &)> describe)
{
    auto *parent = grid->parentWidget();
    InputRow r{name, ui::label(name, "key", parent), ui::label({}, "mono", parent), std::move(active), std::move(describe)};
    r.chip->setAlignment(Qt::AlignCenter);
    r.value->setAlignment((chipOnRight ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    r.value->setMinimumWidth(110);
    const int row = grid->rowCount();
    grid->addWidget(r.chip, row, chipOnRight ? 1 : 0);
    grid->addWidget(r.value, row, chipOnRight ? 0 : 1);
    m_rows.append(r);
}

void ControllerView::tick()
{
    const PadSnapshot snap = drive::Gamepad::instance().snapshot();
    if (snap.connected && snap.digitalMode != m_digitalShown) onPadConnection(true);
    m_pad->setOnline(snap.connected);
    m_pad->setState(snap.input, snap.buttons);
    for (InputRow &r : m_rows) {
        const bool lit = snap.connected && r.active(snap);
        if (lit != r.lit) {
            r.lit = lit;
            r.chip->setProperty("active", lit);
            theme::repolish(r.chip);
        }
        const QString text = snap.connected ? r.describe(snap) : QStringLiteral("–");
        if (text != r.text) r.value->setText(r.text = text);
    }
}

void ControllerView::onPadConnection(bool connected)
{
    auto &pad = drive::Gamepad::instance();
    m_digitalShown = connected && pad.snapshot().digitalMode;
    m_digitalHint->setVisible(m_digitalShown);
    if (!connected) m_padName->setText(tr("Plug in a USB controller and press any button."));
    else if (pad.snapshot().directInput) m_padName->setText(tr("USB gamepad connected. Press buttons and move the sticks to see what each one sends."));
    else m_padName->setText(tr("Controller %1 connected. Press buttons and move the sticks to see what each one sends.").arg(pad.snapshot().slot + 1));
    const auto padSlots = pad.connectedSlots();
    m_padPick->clear();
    for (int s : padSlots) m_padPick->addItem(tr("Controller %1").arg(s + 1), s);
    m_padPick->setVisible(padSlots.size() > 1);
}

void ControllerView::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    m_tick.start();
}

void ControllerView::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    m_tick.stop();
}
