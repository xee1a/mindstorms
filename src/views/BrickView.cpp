#include "BrickView.h"

#include "app/Widgets.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

BrickView::BrickView(QWidget *parent) : QWidget(parent)
{
    auto &brick = nxt::Brick::instance();
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 14);
    lay->setSpacing(14);

    // ── Connection card ───────────────────────────────
    auto *conn = ui::card(this);
    auto *cl = new QHBoxLayout(conn);
    cl->setContentsMargins(26, 26, 26, 26);
    cl->setSpacing(28);
    m_illustration = new ui::BrickIllustration(conn);
    cl->addWidget(m_illustration, 0, Qt::AlignVCenter);

    auto *info = new QVBoxLayout;
    info->setSpacing(10);
    info->addStretch();
    m_title = ui::label({}, "h1", conn);
    m_subtitle = ui::label({}, "muted", conn);
    m_subtitle->setWordWrap(true);
    info->addWidget(m_title);
    info->addWidget(m_subtitle);
    m_usbHint = ui::label({}, "warn", conn);
    m_usbHint->setWordWrap(true);
    info->addWidget(m_usbHint);

    m_facts = new QWidget(conn);
    auto *fg = new QGridLayout(m_facts);
    fg->setContentsMargins(0, 4, 0, 0);
    fg->setHorizontalSpacing(26);
    fg->setVerticalSpacing(2);
    m_firmware = ui::label({}, "mono", m_facts);
    m_flash = ui::label({}, "mono", m_facts);
    m_bluetooth = ui::label({}, "mono", m_facts);
    fg->addWidget(ui::label(tr("FIRMWARE"), "caps", m_facts), 0, 0);
    fg->addWidget(ui::label(tr("FREE FLASH"), "caps", m_facts), 0, 1);
    fg->addWidget(ui::label(tr("BLUETOOTH"), "caps", m_facts), 0, 2);
    fg->addWidget(m_firmware, 1, 0);
    fg->addWidget(m_flash, 1, 1);
    fg->addWidget(m_bluetooth, 1, 2);
    fg->setColumnStretch(3, 1);
    info->addWidget(m_facts);

    m_batteryRow = new QWidget(conn);
    auto *bl = new QHBoxLayout(m_batteryRow);
    bl->setContentsMargins(0, 0, 0, 0);
    m_battery = new ui::BatteryWidget(m_batteryRow);
    m_batteryText = ui::label({}, nullptr, m_batteryRow);
    m_batteryText->setStyleSheet(QStringLiteral("font-weight:600"));
    bl->addWidget(m_battery);
    bl->addWidget(m_batteryText);
    bl->addStretch();
    info->addWidget(m_batteryRow);

    auto *actions = new QHBoxLayout;
    m_connect = ui::button(tr("Connect USB"), "primary", "usb", conn);
    m_demo = ui::button(tr("Try demo brick"), "ghost", "star", conn);
    m_disconnect = ui::button(tr("Disconnect"), "ghost", "unplug", conn);
    actions->addWidget(m_connect);
    actions->addWidget(m_demo);
    actions->addWidget(m_disconnect);
    actions->addStretch();
    info->addLayout(actions);

    m_quick = new QWidget(conn);
    auto *ql = new QHBoxLayout(m_quick);
    ql->setContentsMargins(0, 0, 0, 0);
    auto *beep = ui::button(tr("Beep"), "ghost", "speaker", m_quick);
    auto *stop = ui::button(tr("Stop motors"), "ghost", "square", m_quick);
    ql->addWidget(beep);
    ql->addWidget(stop);
    ql->addStretch();
    info->addWidget(m_quick);
    info->addStretch();
    cl->addLayout(info, 1);
    lay->addWidget(conn, 11);

    connect(m_connect, &QPushButton::clicked, this, [] { nxt::Brick::instance().connectUsb(); });
    connect(m_demo, &QPushButton::clicked, this, [this] {
        m_announcedDemo = false;
        nxt::Brick::instance().connectDemo();
    });
    connect(m_disconnect, &QPushButton::clicked, this, [] { nxt::Brick::instance().disconnectBrick(); });
    connect(beep, &QPushButton::clicked, this, [] {
        nxt::Brick::instance().playTone(659, 150);
        nxt::Brick::instance().playTone(988, 200);
    });
    connect(stop, &QPushButton::clicked, this, [] { nxt::Brick::instance().stopAll(); });

    // ── Driver guide ──────────────────────────────────
    auto *guide = ui::card(this);
    auto *gl = new QVBoxLayout(guide);
    gl->setContentsMargins(22, 22, 22, 22);
    gl->setSpacing(12);
    gl->addWidget(ui::label(tr("Driver setup <span style='color:#6f7f95;font-size:12px;font-weight:600'>(one time)</span>"), "h2", guide));
    auto *intro = ui::label(tr("NXT Studio talks to the brick through Windows' built-in <b>WinUSB</b> driver. "
                               "The NXT doesn't install it automatically, so set it up once:"),
                            "muted", guide);
    intro->setWordWrap(true);
    gl->addWidget(intro);
    const QString steps[] = {
        tr("Plug in the NXT and switch it on."),
        tr("Download <b>Zadig</b> from <span style='font-family:Cascadia Mono'>zadig.akeo.ie</span> and run it."),
        tr("Choose <i>Options → List All Devices</i>, then pick <b>LEGO MINDSTORMS NXT</b> (USB ID <span style='font-family:Cascadia Mono'>0694 0002</span>)."),
        tr("Set the target driver to <b>WinUSB</b> and click <i>Install Driver</i>."),
        tr("Come back here and press <b>Connect USB</b>. After that the app connects automatically whenever the brick is plugged in."),
    };
    for (int i = 0; i < 5; ++i) {
        auto *row = new QHBoxLayout;
        row->setSpacing(12);
        row->addWidget(ui::label(QString::number(i + 1), "step", guide), 0, Qt::AlignTop);
        auto *t = ui::label(steps[i], "muted", guide);
        t->setWordWrap(true);
        row->addWidget(t, 1);
        gl->addLayout(row);
    }

    auto *faqToggle = new QToolButton(guide);
    faqToggle->setText(tr("Troubleshooting"));
    faqToggle->setCheckable(true);
    faqToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    faqToggle->setArrowType(Qt::RightArrow);
    faqToggle->setCursor(Qt::PointingHandCursor);
    faqToggle->setStyleSheet(QStringLiteral("QToolButton{border:none;font-weight:700;padding:6px 0;background:transparent}"));
    auto *faq = ui::label(
        tr("<p><b>Nothing found:</b> try another cable or USB port, and make sure the brick is switched on.</p>"
           "<p><b>Access denied:</b> another program (NXT-G, BricxCC) has the brick open. Close it and try again.</p>"
           "<p><b>Using the old LEGO software too?</b> NXT-G and BricxCC need the Fantom driver. Switch back with Zadig or "
           "Device Manager when you need them.</p>"
           "<p><b>Brick only ticking:</b> it's in firmware-update mode. Reflash the LEGO firmware.</p>"),
        "muted", guide);
    faq->setWordWrap(true);
    faq->hide();
    connect(faqToggle, &QToolButton::toggled, this, [faq, faqToggle](bool open) {
        faq->setVisible(open);
        faqToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    });
    gl->addWidget(faqToggle);
    gl->addWidget(faq);
    gl->addStretch();
    lay->addWidget(guide, 10);

    connect(&brick, &nxt::Brick::stateChanged, this, &BrickView::onBrick);
    onBrick(brick.state());
}

void BrickView::onBrick(const nxt::BrickState &s)
{
    const bool connected = s.status == nxt::LinkStatus::Connected;
    const bool connecting = s.status == nxt::LinkStatus::Connecting;
    m_illustration->setOnline(connected);
    m_title->setText(connected ? s.name : connecting ? tr("Connecting…") : tr("No brick connected"));
    m_subtitle->setText(connected ? (s.demo ? tr("Simulated brick. Everything works except real motors.") : tr("Connected over USB."))
                                  : tr("Connect your NXT with its USB cable and switch it on."));

    const bool needsDriver = !connected && s.usb == nxt::BrickState::Usb::NeedsDriver;
    m_usbHint->setVisible(needsDriver);
    if (needsDriver)
        m_usbHint->setText(tr("An NXT is plugged in, but it's using the “%1” driver. Install WinUSB for it with the steps on the right.")
                               .arg(s.usbDriver.isEmpty() ? tr("none") : s.usbDriver));

    m_facts->setVisible(connected);
    m_firmware->setText(s.firmware);
    m_flash->setText(tr("%1 KB").arg(s.freeFlash / 1024.0, 0, 'f', 1));
    m_bluetooth->setText(s.bluetoothAddress);
    m_batteryRow->setVisible(connected && s.batteryMv.has_value());
    if (s.batteryMv) {
        const int pct = nxt::batteryPercent(*s.batteryMv);
        m_battery->setLevel(pct);
        m_batteryText->setText(QStringLiteral("%1% · %2 V").arg(pct).arg(*s.batteryMv / 1000.0, 0, 'f', 2));
    }
    m_connect->setVisible(!connected);
    m_connect->setEnabled(!connecting);
    m_demo->setVisible(!connected);
    m_disconnect->setVisible(connected);
    m_quick->setVisible(connected);

    if (connected && s.demo && !m_announcedDemo) {
        m_announcedDemo = true;
        emit message(tr("Connected to the demo brick. Motors and sensors are simulated."), "success");
    }
}
