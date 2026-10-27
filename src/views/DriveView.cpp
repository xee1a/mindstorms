#include "DriveView.h"

#include "app/Theme.h"
#include "app/Widgets.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

using drive::DriveMode;
using drive::PadButton;

namespace {

constexpr int TickMs = 20;
const char *const MotorLetters[] = {"A", "B", "C"};

QString modeHint(DriveMode m)
{
    switch (m) {
    case DriveMode::Arcade: return DriveView::tr("Left stick throttle · either stick sideways to steer");
    case DriveMode::Tank: return DriveView::tr("Left stick drives the left motor, right stick the right");
    case DriveMode::Racing: return DriveView::tr("RT forward · LT reverse · left stick steers");
    }
    return {};
}

QComboBox *motorCombo(QWidget *parent, bool allowNone)
{
    auto *c = new QComboBox(parent);
    if (allowNone) c->addItem(DriveView::tr("None"), -1);
    for (int i = 0; i < 3; ++i) c->addItem(DriveView::tr("Motor %1").arg(QLatin1String(MotorLetters[i])), i);
    return c;
}

void selectData(QComboBox *c, int value) { c->setCurrentIndex(std::max(0, c->findData(value))); }

/** Slider with a caption and a live value readout. */
QWidget *slider(QWidget *parent, const QString &caption, int min, int max, int value, const std::function<QString(int)> &fmt,
                const std::function<void(int)> &onChange)
{
    auto *w = new QWidget(parent);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    auto *head = new QHBoxLayout;
    auto *cap = ui::label(caption, "muted", w);
    auto *val = ui::label(fmt(value), nullptr, w);
    val->setStyleSheet(QStringLiteral("font-weight:600"));
    head->addWidget(cap);
    head->addStretch();
    head->addWidget(val);
    v->addLayout(head);
    auto *s = new QSlider(Qt::Horizontal, w);
    s->setRange(min, max);
    s->setValue(value);
    s->setCursor(Qt::PointingHandCursor);
    QObject::connect(s, &QSlider::valueChanged, w, [val, fmt, onChange](int x) {
        val->setText(fmt(x));
        onChange(x);
    });
    v->addWidget(s);
    return w;
}

} // namespace

DriveView::DriveView(QWidget *parent) : QWidget(parent)
{
    loadConfig();
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 14);
    lay->setSpacing(14);
    lay->addWidget(buildControllerCard(), 10);
    lay->addWidget(buildMotorsCard(), 13);
    lay->addWidget(buildSetupCard(), 10);

    m_tick.setInterval(TickMs);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &DriveView::tick);

    auto &pad = drive::Gamepad::instance();
    connect(&pad, &drive::Gamepad::buttonPressed, this, &DriveView::onPadButton);
    connect(&pad, &drive::Gamepad::connectionChanged, this, &DriveView::onPadConnection);
    connect(&nxt::Brick::instance(), &nxt::Brick::stateChanged, this, &DriveView::onBrick);
    connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
        // Key releases aren't delivered while unfocused; drop held keys so the robot doesn't run away.
        if (s != Qt::ApplicationActive) m_keys.clear();
    });
    onBrick(nxt::Brick::instance().state());
    onPadConnection(pad.snapshot().connected);
    updateRoles();
}

QWidget *DriveView::buildControllerCard()
{
    auto *c = ui::card(this);
    auto *v = new QVBoxLayout(c);
    v->setContentsMargins(18, 18, 18, 18);
    v->setSpacing(14);

    auto *head = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(ui::label(tr("Controller"), "h2", c));
    m_padName = ui::label({}, "muted", c);
    m_padName->setWordWrap(true);
    titles->addWidget(m_padName);
    head->addLayout(titles, 1);
    m_padPick = new QComboBox(c);
    m_padPick->setProperty("compact", true);
    m_padPick->hide();
    connect(m_padPick, &QComboBox::activated, this, [this] { drive::Gamepad::instance().setPreferredSlot(m_padPick->currentData().toInt()); });
    head->addWidget(m_padPick, 0, Qt::AlignTop);
    v->addLayout(head);

    auto *stage = ui::inset(c);
    auto *sv = new QVBoxLayout(stage);
    sv->setContentsMargins(10, 16, 10, 8);
    m_controller = new ui::ControllerWidget(stage);
    sv->addWidget(m_controller);
    v->addWidget(stage);

    auto *armRow = new QHBoxLayout;
    armRow->setSpacing(10);
    m_arm = new QPushButton(c);
    m_arm->setObjectName(QStringLiteral("armButton"));
    m_arm->setCheckable(true);
    m_arm->setCursor(Qt::PointingHandCursor);
    m_arm->setMinimumHeight(58);
    connect(m_arm, &QPushButton::clicked, this, [this](bool on) { setArmed(on); });
    auto *stop = ui::button(tr("Stop"), "danger", "square", c);
    stop->setObjectName(QStringLiteral("estop"));
    stop->setMinimumHeight(58);
    stop->setToolTip(tr("Stop all motors (B button or Space)"));
    connect(stop, &QPushButton::clicked, this, &DriveView::emergencyStop);
    armRow->addWidget(m_arm, 1);
    armRow->addWidget(stop);
    v->addLayout(armRow);

    auto *hints = ui::label(tr("<b>A</b> beep &nbsp;·&nbsp; <b>B</b> / <b>Space</b> emergency stop &nbsp;·&nbsp; <b>LB</b> <b>RB</b> aux motor"
                               "<br><b>W A S D</b> or arrows drive without a controller, <b>Q</b> / <b>E</b> aux"),
                            "faint", c);
    hints->setWordWrap(true);
    v->addWidget(hints);
    v->addStretch();
    setArmed(false);
    return c;
}

QWidget *DriveView::buildMotorsCard()
{
    auto *c = ui::card(this);
    auto *v = new QVBoxLayout(c);
    v->setContentsMargins(18, 18, 18, 18);
    v->setSpacing(14);

    auto *head = new QHBoxLayout;
    head->addWidget(ui::label(tr("Motors"), "h2", c));
    head->addStretch();
    m_linkNote = ui::label({}, "faint", c);
    head->addWidget(m_linkNote);
    v->addLayout(head);

    auto *motors = new QHBoxLayout;
    motors->setSpacing(12);
    for (int i = 0; i < 3; ++i) {
        auto *tile = ui::inset(c);
        m_motorTiles[i] = tile;
        auto *tv = new QVBoxLayout(tile);
        tv->setContentsMargins(10, 10, 10, 10);
        tv->setSpacing(4);
        m_gauges[i] = new ui::MotorGauge(tile);
        m_gauges[i]->setCaption(tr("power"));
        tv->addWidget(m_gauges[i], 1);
        tv->addWidget(ui::label(QLatin1String(MotorLetters[i]), "badge", tile), 0, Qt::AlignHCenter);
        m_roles[i] = ui::label({}, "muted", tile);
        m_roles[i]->setAlignment(Qt::AlignCenter);
        m_roles[i]->setWordWrap(true);
        tv->addWidget(m_roles[i]);
        auto *rot = new QHBoxLayout;
        rot->addStretch();
        m_rotation[i] = ui::label(QStringLiteral("–"), "faint", tile);
        rot->addWidget(m_rotation[i]);
        auto *reset = ui::button({}, "ghost", "reset", tile);
        reset->setToolTip(tr("Reset rotation"));
        reset->setFixedSize(28, 26);
        connect(reset, &QPushButton::clicked, this, [i] { nxt::Brick::instance().resetRotation(i); });
        rot->addWidget(reset);
        rot->addStretch();
        tv->addLayout(rot);
        motors->addWidget(tile, 1);
    }
    v->addLayout(motors, 3);

    v->addWidget(ui::label(tr("Sensors"), "h2", c));
    auto *grid = new QGridLayout;
    grid->setSpacing(10);
    for (int p = 0; p < 4; ++p) {
        auto *tile = ui::inset(c);
        auto *tv = new QVBoxLayout(tile);
        tv->setContentsMargins(12, 12, 12, 12);
        tv->setSpacing(8);
        auto *top = new QHBoxLayout;
        top->addWidget(ui::label(QString::number(p + 1), "badge", tile));
        m_sensorKind[p] = new QComboBox(tile);
        m_sensorKind[p]->setProperty("compact", true);
        for (auto k : {nxt::SensorKind::None, nxt::SensorKind::Touch, nxt::SensorKind::Light, nxt::SensorKind::Ambient, nxt::SensorKind::Sound})
            m_sensorKind[p]->addItem(nxt::sensorLabel(k), int(k));
        selectData(m_sensorKind[p], int(nxt::Brick::instance().state().sensors[p]));
        connect(m_sensorKind[p], &QComboBox::activated, this,
                [this, p] { nxt::Brick::instance().setSensor(p, nxt::SensorKind(m_sensorKind[p]->currentData().toInt())); });
        top->addWidget(m_sensorKind[p], 1);
        tv->addLayout(top);
        m_sensorValue[p] = ui::label(QStringLiteral("–"), "value", tile);
        tv->addWidget(m_sensorValue[p]);
        m_sensorBar[p] = new QProgressBar(tile);
        m_sensorBar[p]->setRange(0, 100);
        m_sensorBar[p]->setTextVisible(false);
        tv->addWidget(m_sensorBar[p]);
        grid->addWidget(tile, p / 2, p % 2);
    }
    v->addLayout(grid, 2);
    return c;
}

QWidget *DriveView::buildSetupCard()
{
    auto *c = ui::card(this);
    auto *v = new QVBoxLayout(c);
    v->setContentsMargins(18, 18, 18, 18);
    v->setSpacing(14);
    v->addWidget(ui::label(tr("Drive setup"), "h2", c));

    auto *seg = new QFrame(c);
    seg->setObjectName(QStringLiteral("segmented"));
    auto *sl = new QHBoxLayout(seg);
    sl->setContentsMargins(3, 3, 3, 3);
    sl->setSpacing(2);
    m_modeGroup = new QButtonGroup(this);
    const std::pair<DriveMode, QString> modes[] = {{DriveMode::Arcade, tr("Arcade")}, {DriveMode::Tank, tr("Tank")}, {DriveMode::Racing, tr("Racing")}};
    for (const auto &[mode, name] : modes) {
        auto *b = new QPushButton(name, seg);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(mode == m_cfg.mode);
        m_modeGroup->addButton(b, int(mode));
        sl->addWidget(b);
    }
    v->addWidget(seg);
    m_modeHint = ui::label(modeHint(m_cfg.mode), "faint", c);
    m_modeHint->setWordWrap(true);
    v->addWidget(m_modeHint);
    connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_cfg.mode = DriveMode(id);
        m_modeHint->setText(modeHint(m_cfg.mode));
        saveConfig();
    });

    auto *ports = new QGridLayout;
    ports->setHorizontalSpacing(10);
    ports->setVerticalSpacing(6);
    m_left = motorCombo(c, false);
    m_right = motorCombo(c, false);
    m_aux = motorCombo(c, true);
    selectData(m_left, m_cfg.leftMotor);
    selectData(m_right, m_cfg.rightMotor);
    selectData(m_aux, m_cfg.auxMotor.value_or(-1));
    ports->addWidget(ui::label(tr("Left side"), "muted", c), 0, 0);
    ports->addWidget(ui::label(tr("Right side"), "muted", c), 0, 1);
    ports->addWidget(ui::label(tr("Aux (LB/RB)"), "muted", c), 0, 2);
    ports->addWidget(m_left, 1, 0);
    ports->addWidget(m_right, 1, 1);
    ports->addWidget(m_aux, 1, 2);
    v->addLayout(ports);
    const auto onPorts = [this] {
        m_cfg.leftMotor = m_left->currentData().toInt();
        m_cfg.rightMotor = m_right->currentData().toInt();
        const int aux = m_aux->currentData().toInt();
        m_cfg.auxMotor = aux < 0 ? std::nullopt : std::optional<int>(aux);
        saveConfig();
        updateRoles();
    };
    for (QComboBox *cb : {m_left, m_right, m_aux}) connect(cb, &QComboBox::activated, this, onPorts);

    const auto makeToggle = [&](const QString &text, bool checked, std::function<void(bool)> set) {
        auto *t = new ui::ToggleSwitch(text, c);
        t->setChecked(checked);
        connect(t, &ui::ToggleSwitch::toggled, this, [this, set](bool on) {
            set(on);
            saveConfig();
        });
        return t;
    };
    const auto addToggle = [&](QBoxLayout *l, const QString &text, bool checked, std::function<void(bool)> set) {
        l->addWidget(makeToggle(text, checked, std::move(set)));
    };
    // One "reverse" switch under each port picker; the column header says which side.
    ports->addWidget(makeToggle(tr("Reverse"), m_cfg.invertLeft, [this](bool on) { m_cfg.invertLeft = on; }), 2, 0);
    ports->addWidget(makeToggle(tr("Reverse"), m_cfg.invertRight, [this](bool on) { m_cfg.invertRight = on; }), 2, 1);
    ports->addWidget(makeToggle(tr("Reverse"), m_cfg.invertAux, [this](bool on) { m_cfg.invertAux = on; }), 2, 2);
    ports->setRowMinimumHeight(2, 30);

    const auto pct = [](int x) { return QStringLiteral("%1%").arg(x); };
    const auto frac = [](int x) { return QString::number(x / 100.0, 'f', 2); };
    v->addWidget(slider(c, tr("Top speed"), 10, 100, m_cfg.maxPower, pct, [this](int x) { m_cfg.maxPower = x; saveConfig(); }));
    v->addWidget(slider(c, tr("Aux power"), 10, 100, m_cfg.auxPower, pct, [this](int x) { m_cfg.auxPower = x; saveConfig(); }));
    v->addWidget(slider(c, tr("Stick deadzone"), 0, 40, int(m_cfg.deadzone * 100), frac,
                        [this](int x) { m_cfg.deadzone = x / 100.0; saveConfig(); }));
    v->addWidget(slider(c, tr("Fine control (expo)"), 0, 100, int(m_cfg.expo * 100), frac,
                        [this](int x) { m_cfg.expo = x / 100.0; saveConfig(); }));

    auto *motorOpts = new QHBoxLayout;
    motorOpts->setSpacing(16);
    auto &brick = nxt::Brick::instance();
    addToggle(motorOpts, tr("Brake when stopped"), brick.brakeOnStop(), [](bool on) { nxt::Brick::instance().setBrakeOnStop(on); });
    addToggle(motorOpts, tr("Speed regulation"), brick.speedRegulation(), [](bool on) { nxt::Brick::instance().setSpeedRegulation(on); });
    motorOpts->addStretch();
    v->addLayout(motorOpts);
    v->addStretch();
    return c;
}

void DriveView::loadConfig()
{
    QSettings s;
    s.beginGroup(QStringLiteral("drive"));
    m_cfg.mode = DriveMode(std::clamp(s.value("mode", int(m_cfg.mode)).toInt(), 0, 2));
    m_cfg.leftMotor = std::clamp(s.value("leftMotor", m_cfg.leftMotor).toInt(), 0, 2);
    m_cfg.rightMotor = std::clamp(s.value("rightMotor", m_cfg.rightMotor).toInt(), 0, 2);
    const int aux = s.value("auxMotor", m_cfg.auxMotor.value_or(-1)).toInt();
    m_cfg.auxMotor = aux < 0 || aux > 2 ? std::nullopt : std::optional<int>(aux);
    m_cfg.invertLeft = s.value("invertLeft", false).toBool();
    m_cfg.invertRight = s.value("invertRight", false).toBool();
    m_cfg.invertAux = s.value("invertAux", false).toBool();
    m_cfg.maxPower = s.value("maxPower", m_cfg.maxPower).toInt();
    m_cfg.auxPower = s.value("auxPower", m_cfg.auxPower).toInt();
    m_cfg.deadzone = s.value("deadzone", m_cfg.deadzone).toDouble();
    m_cfg.expo = s.value("expo", m_cfg.expo).toDouble();
}

void DriveView::saveConfig()
{
    QSettings s;
    s.beginGroup(QStringLiteral("drive"));
    s.setValue("mode", int(m_cfg.mode));
    s.setValue("leftMotor", m_cfg.leftMotor);
    s.setValue("rightMotor", m_cfg.rightMotor);
    s.setValue("auxMotor", m_cfg.auxMotor.value_or(-1));
    s.setValue("invertLeft", m_cfg.invertLeft);
    s.setValue("invertRight", m_cfg.invertRight);
    s.setValue("invertAux", m_cfg.invertAux);
    s.setValue("maxPower", m_cfg.maxPower);
    s.setValue("auxPower", m_cfg.auxPower);
    s.setValue("deadzone", m_cfg.deadzone);
    s.setValue("expo", m_cfg.expo);
}

void DriveView::updateRoles()
{
    for (int i = 0; i < 3; ++i) {
        QStringList roles;
        if (m_cfg.leftMotor == i) roles << tr("Left drive");
        if (m_cfg.rightMotor == i && m_cfg.rightMotor != m_cfg.leftMotor) roles << tr("Right drive");
        if (m_cfg.auxMotor == i && i != m_cfg.leftMotor && i != m_cfg.rightMotor) roles << tr("Aux");
        m_roles[i]->setText(roles.isEmpty() ? tr("Unused") : roles.join(QStringLiteral(" · ")));
        auto *fx = m_motorTiles[i];
        fx->setEnabled(!roles.isEmpty());
    }
}

void DriveView::setArmed(bool on)
{
    m_armed = on;
    m_arm->setChecked(on);
    m_arm->setText(on ? tr("●  Driving\n     Start or B to stop") : tr("○  Start driving\n     Start button or click"));
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
    const auto &pad = drive::Gamepad::instance().snapshot();
    const drive::ControlInput input = pad.connected ? pad.input : keyboardInput();
    m_controller->setState(input, pad.buttons, pad.connected || !m_keys.isEmpty());
    if (m_armed) nxt::Brick::instance().setMotors(drive::mix(input, m_cfg));
}

void DriveView::onPadButton(PadButton b)
{
    if (!isVisible()) return;
    if (b == PadButton::Start) setArmed(!m_armed);
    else if (b == PadButton::B) emergencyStop();
    else if (b == PadButton::A) nxt::Brick::instance().playTone(880, 120);
}

void DriveView::onPadConnection(bool connected)
{
    auto &pad = drive::Gamepad::instance();
    m_padName->setText(connected ? tr("Xbox-compatible controller (XInput #%1)").arg(pad.snapshot().slot + 1)
                                 : tr("Plug in a USB controller and press any button"));
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
    for (int i = 0; i < 3; ++i) {
        m_gauges[i]->setPower(s.motorPower[i]);
        m_rotation[i]->setText(connected ? QStringLiteral("%1°").arg(s.motorRotation[i]) : QStringLiteral("–"));
    }
    m_linkNote->setText(connected ? (s.demo ? tr("Demo brick (simulated)") : tr("Live on %1").arg(s.name))
                                  : tr("Preview only, no brick connected"));
    for (int p = 0; p < 4; ++p) {
        selectData(m_sensorKind[p], int(s.sensors[p]));
        const auto v = s.sensorValues[p];
        const bool idle = s.sensors[p] == nxt::SensorKind::None || !v;
        m_sensorValue[p]->setEnabled(!idle);
        if (idle) {
            m_sensorValue[p]->setText(QStringLiteral("–"));
            m_sensorBar[p]->setValue(0);
        } else if (s.sensors[p] == nxt::SensorKind::Touch) {
            m_sensorValue[p]->setText(*v ? tr("Pressed") : tr("Released"));
            m_sensorBar[p]->setValue(*v ? 100 : 0);
        } else {
            const QString unit = QString::fromLatin1(nxt::sensorInfo(s.sensors[p]).unit);
            m_sensorValue[p]->setText(unit.isEmpty() ? QString::number(*v) : QStringLiteral("%1 %2").arg(*v).arg(unit));
            m_sensorBar[p]->setValue(std::clamp(*v, 0, 100));
        }
    }
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
