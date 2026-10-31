#include "DriveView.h"

#include "app/Prefs.h"
#include "app/Theme.h"
#include "app/Widgets.h"

#include <QApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

using drive::DriveMode;
using drive::PadButton;
using drive::PadSnapshot;

namespace {

constexpr int TickMs = 20;

QString modeName(DriveMode m)
{
    switch (m) {
    case DriveMode::Arcade: return DriveView::tr("Arcade");
    case DriveMode::Tank: return DriveView::tr("Tank");
    case DriveMode::Racing: return DriveView::tr("Racing");
    }
    return {};
}

} // namespace

DriveView::DriveView(QWidget *parent) : QWidget(parent)
{
    m_cfg = Prefs::instance().driveConfig();
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 14);

    auto *card = ui::card(this);
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(10);
    lay->addWidget(card);

    // ── Header: controller name and live status ─────────
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
    m_modeNote = ui::label({}, "pill", card);
    head->addWidget(m_modeNote, 0, Qt::AlignVCenter);
    m_brickNote = ui::label({}, "pill", card);
    head->addWidget(m_brickNote, 0, Qt::AlignVCenter);
    v->addLayout(head);

    // ── Inputs | photo | inputs ─────────────────────────
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
    const auto aux = [this](const char *dir) {
        return [this, dir] {
            if (!m_cfg.auxMotor) return tr("No extra motor set");
            return QLatin1String(dir) == QLatin1String("back") ? tr("Motor %1 backwards").arg(motorName(m_cfg.auxMotor))
                                                               : tr("Motor %1 forwards").arg(motorName(m_cfg.auxMotor));
        };
    };
    const auto unused = [] { return tr("Not used in this style"); };

    addInput(left, true, QStringLiteral("LT"), [](const PadSnapshot &s) { return s.input.leftTrigger > 0.05; },
             [this, unused] { return m_cfg.mode == DriveMode::Racing ? tr("Reverse") : unused(); });
    addInput(left, true, QStringLiteral("LB"), [](const PadSnapshot &s) { return s.input.leftBumper; }, aux("back"));
    addInput(left, true, tr("Left stick"), [this](const PadSnapshot &s) { return std::hypot(s.input.leftX, s.input.leftY) > m_cfg.deadzone; },
             [this] {
                 switch (m_cfg.mode) {
                 case DriveMode::Arcade: return tr("Drive forward/back and steer");
                 case DriveMode::Tank: return tr("Left side (motor %1)").arg(motorName(m_cfg.leftMotor));
                 case DriveMode::Racing: return tr("Steer");
                 }
                 return QString();
             });
    addInput(left, true, QStringLiteral("L3"), pressed(PadButton::LS), [] { return tr("Turbo: full power while held"); });
    addInput(left, true, QStringLiteral("↑ ↓"), [](const PadSnapshot &s) { return s.pressed(PadButton::Up) || s.pressed(PadButton::Down); },
             [this] { return tr("Top speed ±10% (now %1%)").arg(m_cfg.maxPower); });
    addInput(left, true, QStringLiteral("← →"), [](const PadSnapshot &s) { return s.pressed(PadButton::Left) || s.pressed(PadButton::Right); },
             [this] { return tr("Extra motor power ±10% (now %1%)").arg(m_cfg.auxPower); });
    addInput(left, true, QStringLiteral("SELECT"), pressed(PadButton::Back), [] { return tr("Switch control style"); });
    addInput(left, true, QStringLiteral("START"), pressed(PadButton::Start), [] { return tr("Start / pause driving"); });

    addInput(right, false, QStringLiteral("RT"), [](const PadSnapshot &s) { return s.input.rightTrigger > 0.05; },
             [this, unused] { return m_cfg.mode == DriveMode::Racing ? tr("Accelerate") : unused(); });
    addInput(right, false, QStringLiteral("RB"), [](const PadSnapshot &s) { return s.input.rightBumper; }, aux("forward"));
    addInput(right, false, tr("Right stick"), [this](const PadSnapshot &s) { return std::hypot(s.input.rightX, s.input.rightY) > m_cfg.deadzone; },
             [this, unused] {
                 switch (m_cfg.mode) {
                 case DriveMode::Arcade: return tr("Steer");
                 case DriveMode::Tank: return tr("Right side (motor %1)").arg(motorName(m_cfg.rightMotor));
                 case DriveMode::Racing: return unused();
                 }
                 return QString();
             });
    addInput(right, false, QStringLiteral("R3"), pressed(PadButton::RS),
             [this] { return m_precision ? tr("Precision mode: on (half power)") : tr("Precision mode: off"); });
    addInput(right, false, QStringLiteral("Y"), pressed(PadButton::Y),
             [] { return nxt::Brick::instance().brakeOnStop() ? tr("Brake when stopped: on") : tr("Brake when stopped: off"); });
    addInput(right, false, QStringLiteral("X"), pressed(PadButton::X), [] { return tr("Reset rotation counters"); });
    addInput(right, false, QStringLiteral("B"), pressed(PadButton::B), [] { return tr("Emergency stop"); });
    addInput(right, false, QStringLiteral("A"), pressed(PadButton::A), [] { return tr("Horn"); });

    // ── Drive controls ──────────────────────────────────
    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    actions->addStretch();
    m_arm = new QPushButton(card);
    m_arm->setObjectName(QStringLiteral("armButton"));
    m_arm->setCheckable(true);
    m_arm->setCursor(Qt::PointingHandCursor);
    m_arm->setMinimumSize(240, 52);
    connect(m_arm, &QPushButton::clicked, this, [this](bool on) { setArmed(on); });
    auto *stop = ui::button(tr("Stop"), "danger", "square", card);
    stop->setObjectName(QStringLiteral("estop"));
    stop->setMinimumHeight(52);
    stop->setToolTip(tr("Stop all motors (B or Space)"));
    connect(stop, &QPushButton::clicked, this, &DriveView::emergencyStop);
    actions->addWidget(m_arm);
    actions->addWidget(stop);
    actions->addStretch();
    v->addLayout(actions);
    auto *keys = ui::label(tr("No controller? Drive with <b>W A S D</b> or the arrows, <b>Q</b> / <b>E</b> for the extra motor, "
                              "<b>Space</b> stops. Change ports and feel in Settings → Drive."),
                           "faint", card);
    keys->setAlignment(Qt::AlignCenter);
    keys->setWordWrap(true);
    v->addWidget(keys);

    m_tick.setInterval(TickMs);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &DriveView::tick);

    auto &pad = drive::Gamepad::instance();
    connect(&pad, &drive::Gamepad::buttonPressed, this, &DriveView::onPadButton);
    connect(&pad, &drive::Gamepad::connectionChanged, this, &DriveView::onPadConnection);
    connect(&nxt::Brick::instance(), &nxt::Brick::stateChanged, this, &DriveView::onBrick);
    connect(&Prefs::instance(), &Prefs::driveConfigChanged, this, &DriveView::setConfig);
    connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
        // Key releases aren't delivered while unfocused; drop held keys so the robot doesn't run away.
        if (s != Qt::ApplicationActive) m_keys.clear();
    });
    setArmed(false);
    onBrick(nxt::Brick::instance().state());
    onPadConnection(pad.snapshot().connected);
    refreshDescriptions();
}

void DriveView::addInput(QGridLayout *grid, bool chipOnRight, const QString &name, std::function<bool(const PadSnapshot &)> active,
                         std::function<QString()> describe)
{
    auto *parent = grid->parentWidget();
    InputRow r{ui::label(name, "key", parent), ui::label({}, "muted", parent), std::move(active), std::move(describe)};
    r.chip->setAlignment(Qt::AlignCenter);
    r.action->setAlignment((chipOnRight ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    const int row = grid->rowCount();
    grid->addWidget(r.chip, row, chipOnRight ? 1 : 0);
    grid->addWidget(r.action, row, chipOnRight ? 0 : 1);
    m_rows.append(r);
}

void DriveView::refreshDescriptions()
{
    for (InputRow &r : m_rows) r.action->setText(r.describe());
    QString mode = tr("Style: %1").arg(modeName(m_cfg.mode));
    if (m_precision) mode += tr(" · precision");
    m_modeNote->setText(mode);
}

void DriveView::setConfig(const drive::DriveConfig &cfg)
{
    m_cfg = cfg;
    refreshDescriptions();
}

QString DriveView::motorName(std::optional<int> port) const
{
    static const char *const letters[] = {"A", "B", "C"};
    return port && *port >= 0 && *port < 3 ? QLatin1String(letters[*port]) : QStringLiteral("–");
}

void DriveView::announce(const QString &text) { emit message(text, "info"); }

void DriveView::setArmed(bool on)
{
    m_armed = on;
    m_arm->setChecked(on);
    m_arm->setText(on ? tr("●  Driving · START to pause") : tr("○  Start driving · START"));
    if (!on) nxt::Brick::instance().setMotors({0, 0, 0});
}

void DriveView::emergencyStop()
{
    setArmed(false);
    nxt::Brick::instance().stopAll();
}

drive::ControlInput DriveView::keyboardInput() const
{
    const auto k = [this](std::initializer_list<int> keys) {
        for (int key : keys)
            if (m_keys.contains(key)) return 1.0;
        return 0.0;
    };
    drive::ControlInput in;
    const double y = k({Qt::Key_W, Qt::Key_Up}) - k({Qt::Key_S, Qt::Key_Down});
    const double x = k({Qt::Key_D, Qt::Key_Right}) - k({Qt::Key_A, Qt::Key_Left});
    in.leftY = y;
    in.rightY = y;
    in.leftX = x * 0.7;
    in.leftBumper = m_keys.contains(Qt::Key_Q);
    in.rightBumper = m_keys.contains(Qt::Key_E);
    return in;
}

void DriveView::tick()
{
    PadSnapshot snap = drive::Gamepad::instance().snapshot();
    if (!snap.connected) snap.input = keyboardInput();
    if (snap.digitalMode != m_digitalShown) onPadConnection(snap.connected);
    m_pad->setOnline(snap.connected);
    m_pad->setState(snap.input, snap.buttons);
    for (InputRow &r : m_rows) {
        const bool lit = r.active(snap);
        if (lit == r.lit) continue;
        r.lit = lit;
        r.chip->setProperty("active", lit);
        theme::repolish(r.chip);
    }
    if (!m_armed) return;
    drive::DriveConfig cfg = m_cfg;
    if (snap.pressed(PadButton::LS)) cfg.maxPower = 100;
    else if (m_precision) cfg.maxPower = std::max(10, cfg.maxPower / 2);
    nxt::Brick::instance().setMotors(drive::mix(snap.input, cfg));
}

void DriveView::onPadButton(PadButton b)
{
    if (!isVisible()) return;
    auto &brick = nxt::Brick::instance();
    auto &prefs = Prefs::instance();
    drive::DriveConfig cfg = m_cfg;
    switch (b) {
    case PadButton::Start: setArmed(!m_armed); return;
    case PadButton::B: emergencyStop(); return;
    case PadButton::A: brick.playTone(880, 150); return;
    case PadButton::X:
        for (int i = 0; i < 3; ++i) brick.resetRotation(i);
        announce(tr("Rotation counters reset"));
        return;
    case PadButton::Y:
        brick.setBrakeOnStop(!brick.brakeOnStop());
        announce(brick.brakeOnStop() ? tr("Brake when stopped: on") : tr("Brake when stopped: off"));
        refreshDescriptions();
        return;
    case PadButton::RS:
        m_precision = !m_precision;
        announce(m_precision ? tr("Precision mode: on (half power)") : tr("Precision mode: off"));
        refreshDescriptions();
        return;
    case PadButton::Back:
        cfg.mode = DriveMode((int(cfg.mode) + 1) % 3);
        announce(tr("Control style: %1").arg(modeName(cfg.mode)));
        break;
    case PadButton::Up:
    case PadButton::Down:
        cfg.maxPower = std::clamp(cfg.maxPower + (b == PadButton::Up ? 10 : -10), 10, 100);
        announce(tr("Top speed: %1%").arg(cfg.maxPower));
        break;
    case PadButton::Left:
    case PadButton::Right:
        cfg.auxPower = std::clamp(cfg.auxPower + (b == PadButton::Right ? 10 : -10), 10, 100);
        announce(tr("Extra motor power: %1%").arg(cfg.auxPower));
        break;
    default: return;
    }
    prefs.setDriveConfig(cfg); // comes back through driveConfigChanged
}

void DriveView::onPadConnection(bool connected)
{
    auto &pad = drive::Gamepad::instance();
    m_digitalShown = connected && pad.snapshot().digitalMode;
    if (!connected) m_padName->setText(tr("Plug in a USB controller and press any button."));
    else if (pad.snapshot().digitalMode)
        m_padName->setText(tr("The pad is in digital mode: the right stick works like the A, B, X and Y buttons. "
                              "Press ANALOG (or MODE) on the pad so its light comes on."));
    else if (pad.snapshot().directInput)
        m_padName->setText(tr("USB gamepad connected. Press START to drive."));
    else m_padName->setText(tr("Controller %1 connected. Press START to drive.").arg(pad.snapshot().slot + 1));
    const auto padSlots = pad.connectedSlots();
    m_padPick->clear();
    for (int s : padSlots) m_padPick->addItem(tr("Controller %1").arg(s + 1), s);
    m_padPick->setVisible(padSlots.size() > 1);
    if (!connected && m_armed && m_keys.isEmpty()) {
        emergencyStop();
        emit message(tr("Controller disconnected, so the motors were stopped."), "error");
    }
}

void DriveView::onBrick(const nxt::BrickState &s)
{
    const bool connected = s.status == nxt::LinkStatus::Connected;
    m_brickNote->setText(connected ? tr("Brick: %1").arg(s.name) : tr("No brick: preview only"));
    m_brickNote->setProperty("ok", connected);
    theme::repolish(m_brickNote);
}

void DriveView::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    qApp->installEventFilter(this);
    m_tick.start();
}

void DriveView::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    qApp->removeEventFilter(this);
    m_tick.stop();
    m_keys.clear();
    setArmed(false);
}

bool DriveView::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() != QEvent::KeyPress && e->type() != QEvent::KeyRelease) return QWidget::eventFilter(o, e);
    auto *ke = static_cast<QKeyEvent *>(e);
    QWidget *focus = QApplication::focusWidget();
    const bool typing = qobject_cast<QLineEdit *>(focus) || qobject_cast<QComboBox *>(focus) || qobject_cast<QSlider *>(focus);
    static const QSet<int> driveKeys{Qt::Key_W, Qt::Key_A, Qt::Key_S, Qt::Key_D, Qt::Key_Up, Qt::Key_Down,
                                     Qt::Key_Left, Qt::Key_Right, Qt::Key_Q, Qt::Key_E};
    if (ke->key() == Qt::Key_Space && e->type() == QEvent::KeyPress) {
        emergencyStop();
        return true;
    }
    if (typing || !driveKeys.contains(ke->key()) || ke->isAutoRepeat()) return QWidget::eventFilter(o, e);
    if (e->type() == QEvent::KeyPress) m_keys.insert(ke->key());
    else m_keys.remove(ke->key());
    return true;
}
