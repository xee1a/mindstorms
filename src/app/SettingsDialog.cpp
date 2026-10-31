#include "SettingsDialog.h"
#include "FactoryResetDialog.h"

#include "Prefs.h"
#include "Theme.h"
#include "Widgets.h"
#include "nxt/Brick.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

namespace {

const char *const MotorLetters[] = {"A", "B", "C"};

QString setting(const char *key, const char *fallback) { return QSettings().value(QLatin1String(key), QLatin1String(fallback)).toString(); }

/** A titled group of settings rows. */
QFrame *section(QWidget *parent, const QString &title, QVBoxLayout **body, const QString &subtitle = {})
{
    auto *f = ui::inset(parent);
    auto *v = new QVBoxLayout(f);
    v->setContentsMargins(18, 16, 18, 18);
    v->setSpacing(12);
    v->addWidget(ui::label(title, "h2", f));
    if (!subtitle.isEmpty()) {
        auto *s = ui::label(subtitle, "faint", f);
        s->setWordWrap(true);
        v->addWidget(s);
    }
    *body = v;
    return f;
}

/** Label on the left, control on the right. */
QHBoxLayout *row(QWidget *parent, const QString &text, QWidget *control, const QString &hint = {})
{
    auto *h = new QHBoxLayout;
    h->setSpacing(16);
    auto *texts = new QVBoxLayout;
    texts->setSpacing(1);
    texts->addWidget(ui::label(text, nullptr, parent));
    if (!hint.isEmpty()) {
        auto *l = ui::label(hint, "faint", parent);
        l->setWordWrap(true);
        texts->addWidget(l);
    }
    h->addLayout(texts, 1);
    h->addWidget(control, 0, Qt::AlignVCenter);
    return h;
}

ui::ToggleSwitch *toggle(QWidget *parent, bool on, std::function<void(bool)> set)
{
    auto *t = new ui::ToggleSwitch({}, parent);
    t->setChecked(on);
    QObject::connect(t, &ui::ToggleSwitch::toggled, parent, std::move(set));
    return t;
}

/** Segmented control; returns the group so callers can read the id. */
QFrame *segmented(QWidget *parent, const QList<QPair<QString, int>> &items, int checked, std::function<void(int)> onPick)
{
    auto *seg = new QFrame(parent);
    seg->setObjectName(QStringLiteral("segmented"));
    auto *l = new QHBoxLayout(seg);
    l->setContentsMargins(3, 3, 3, 3);
    l->setSpacing(2);
    auto *group = new QButtonGroup(seg);
    for (const auto &[text, id] : items) {
        auto *b = new QPushButton(text, seg);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(id == checked);
        group->addButton(b, id);
        l->addWidget(b);
    }
    QObject::connect(group, &QButtonGroup::idClicked, seg, std::move(onPick));
    return seg;
}

QComboBox *motorCombo(QWidget *parent, bool allowNone, int value)
{
    auto *c = new QComboBox(parent);
    if (allowNone) c->addItem(SettingsDialog::tr("None"), -1);
    for (int i = 0; i < 3; ++i) c->addItem(SettingsDialog::tr("Motor %1").arg(QLatin1String(MotorLetters[i])), i);
    c->setCurrentIndex(std::max(0, c->findData(value)));
    return c;
}

/** Slider with a caption and a live value readout. */
QWidget *slider(QWidget *parent, const QString &caption, int min, int max, int value, const std::function<QString(int)> &fmt,
                const std::function<void(int)> &onChange)
{
    auto *w = new QWidget(parent);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    auto *head = new QHBoxLayout;
    auto *val = ui::label(fmt(value), nullptr, w);
    val->setStyleSheet(QStringLiteral("font-weight:600"));
    head->addWidget(ui::label(caption, "muted", w));
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

QString modeHint(drive::DriveMode m)
{
    switch (m) {
    case drive::DriveMode::Arcade: return SettingsDialog::tr("Left stick drives forward and back, either stick steers.");
    case drive::DriveMode::Tank: return SettingsDialog::tr("Left stick drives the left side, right stick the right side.");
    case drive::DriveMode::Racing: return SettingsDialog::tr("RT accelerates, LT reverses, left stick steers.");
    }
    return {};
}

/** Page body inside a scroll area, so long pages still fit small windows. */
QWidget *page(QWidget *parent, QVBoxLayout **body)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget(scroll);
    auto *v = new QVBoxLayout(inner);
    v->setContentsMargins(4, 0, 12, 0);
    v->setSpacing(14);
    scroll->setWidget(inner);
    *body = v;
    return scroll;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("settings"));
    setWindowTitle(tr("Settings"));
    resize(820, 600);
    m_startTheme = setting("appearance/theme", "dark");
    m_startAccent = setting("appearance/accent", "blue");
    m_startLanguage = setting("language", "pl");
    m_drive = Prefs::instance().driveConfig();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);
    outer->setSpacing(14);

    m_restartBanner = new QFrame(this);
    m_restartBanner->setObjectName(QStringLiteral("restartBanner"));
    auto *rb = new QHBoxLayout(m_restartBanner);
    rb->setContentsMargins(14, 8, 8, 8);
    auto *rbText = ui::label(tr("Restart NXT Studio to apply the new look or language."), nullptr, m_restartBanner);
    rbText->setWordWrap(true);
    rb->addWidget(rbText, 1);
    auto *restart = ui::button(tr("Restart now"), "primary", "reset", m_restartBanner);
    connect(restart, &QPushButton::clicked, this, [this] {
        accept();
        emit restartRequested();
    });
    rb->addWidget(restart);
    m_restartBanner->hide();
    outer->addWidget(m_restartBanner);

    auto *body = new QHBoxLayout;
    body->setSpacing(18);
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("settingsNav"));
    m_nav->setFixedWidth(190);
    m_nav->setIconSize(QSize(18, 18));
    m_nav->setFocusPolicy(Qt::NoFocus);
    m_pages = new QStackedWidget(this);
    body->addWidget(m_nav);
    body->addWidget(m_pages, 1);
    outer->addLayout(body, 1);

    addPage("star", tr("Appearance"), buildAppearance());
    addPage("blocks", tr("Editor"), buildEditor());
    addPage("gamepad", tr("Drive"), buildDrive());
    addPage("brick", tr("Brick"), buildBrick());
    addPage("gear", tr("About"), buildAbout());
    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *close = ui::button(tr("Done"), "primary", nullptr, this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    outer->addLayout(buttons);
}

void SettingsDialog::addPage(const char *icon, const QString &title, QWidget *page)
{
    auto *item = new QListWidgetItem(ui::icon(icon), title, m_nav);
    item->setSizeHint(QSize(0, 40));
    m_pages->addWidget(page);
}

void SettingsDialog::updateRestartBanner()
{
    const bool differs = setting("appearance/theme", "dark") != m_startTheme || setting("appearance/accent", "blue") != m_startAccent ||
                         setting("language", "pl") != m_startLanguage;
    m_restartBanner->setVisible(differs);
}

QWidget *SettingsDialog::buildAppearance()
{
    QVBoxLayout *v;
    QWidget *p = page(this, &v);

    QVBoxLayout *look;
    v->addWidget(section(p, tr("Theme"), &look));
    const QStringList themeIds{QStringLiteral("dark"), QStringLiteral("light"), QStringLiteral("system")};
    auto *themes = segmented(p, {{tr("Dark"), 0}, {tr("Light"), 1}, {tr("Match Windows"), 2}}, int(themeIds.indexOf(m_startTheme)),
                             [this, themeIds](int id) {
                                 QSettings().setValue(QStringLiteral("appearance/theme"), themeIds.value(id));
                                 updateRestartBanner();
                             });
    look->addLayout(row(p, tr("Colour scheme"), themes));

    auto *swatches = new QWidget(p);
    auto *sl = new QHBoxLayout(swatches);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(8);
    auto *group = new QButtonGroup(swatches);
    const auto choices = theme::accents();
    for (int i = 0; i < choices.size(); ++i) {
        const auto &a = choices[i];
        auto *b = new QPushButton(swatches);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(a.name);
        b->setFixedSize(30, 30);
        b->setStyleSheet(QStringLiteral("QPushButton{background:%1;border:3px solid transparent;border-radius:15px;padding:0}"
                                        "QPushButton:hover{border-color:%2}QPushButton:checked{border-color:%3}")
                             .arg(a.swatch.name(), theme::Border.name(), theme::Text.name()));
        b->setChecked(a.id == m_startAccent);
        group->addButton(b, i);
        sl->addWidget(b);
    }
    connect(group, &QButtonGroup::idClicked, this, [this, choices](int id) {
        QSettings().setValue(QStringLiteral("appearance/accent"), choices[id].id);
        updateRestartBanner();
    });
    look->addLayout(row(p, tr("Accent colour"), swatches, tr("Used for buttons, switches and highlights.")));

    QVBoxLayout *lang;
    v->addWidget(section(p, tr("Language"), &lang));
    auto *combo = new QComboBox(p);
    combo->addItem(QStringLiteral("Polski"), QStringLiteral("pl"));
    combo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    combo->setCurrentIndex(std::max(0, combo->findData(m_startLanguage)));
    connect(combo, &QComboBox::activated, this, [this, combo] {
        QSettings().setValue(QStringLiteral("language"), combo->currentData());
        updateRestartBanner();
    });
    lang->addLayout(row(p, tr("App language"), combo));
    v->addStretch();
    return p;
}

QWidget *SettingsDialog::buildEditor()
{
    auto &prefs = Prefs::instance();
    QVBoxLayout *v;
    QWidget *p = page(this, &v);

    QVBoxLayout *ed;
    v->addWidget(section(p, tr("Block editor"), &ed));
    ed->addLayout(row(p, tr("Dot grid"), toggle(p, prefs.showGrid(), [](bool on) { Prefs::instance().setShowGrid(on); }),
                      tr("Dots in the background of the canvas.")));
    ed->addLayout(row(p, tr("Sounds"), toggle(p, prefs.editorSounds(), [](bool on) { Prefs::instance().setEditorSounds(on); }),
                      tr("Quiet clicks when you pick up, attach and delete blocks.")));
    ed->addLayout(row(p, tr("Autosave"), toggle(p, prefs.autosave(), [](bool on) { Prefs::instance().setAutosave(on); }),
                      tr("Keeps your program safe between sessions, even without saving a file.")));

    QVBoxLayout *start;
    v->addWidget(section(p, tr("Startup"), &start));
    auto *tab = new QComboBox(p);
    tab->addItem(tr("Last used"), -1);
    tab->addItem(tr("Program"), 0);
    tab->addItem(tr("Drive"), 1);
    tab->addItem(tr("Brick"), 2);
    tab->setCurrentIndex(std::max(0, tab->findData(prefs.startTab())));
    connect(tab, &QComboBox::activated, this, [tab] { Prefs::instance().setStartTab(tab->currentData().toInt()); });
    start->addLayout(row(p, tr("Open on"), tab));
    v->addStretch();
    return p;
}

void SettingsDialog::saveDrive() { Prefs::instance().setDriveConfig(m_drive); }

QWidget *SettingsDialog::buildDrive()
{
    using drive::DriveMode;
    QVBoxLayout *v;
    QWidget *p = page(this, &v);

    QVBoxLayout *mode;
    v->addWidget(section(p, tr("Control style"), &mode));
    mode->addWidget(segmented(p, {{tr("Arcade"), int(DriveMode::Arcade)}, {tr("Tank"), int(DriveMode::Tank)}, {tr("Racing"), int(DriveMode::Racing)}},
                              int(m_drive.mode), [this](int id) {
                                  m_drive.mode = DriveMode(id);
                                  m_modeHint->setText(modeHint(m_drive.mode));
                                  saveDrive();
                              }));
    m_modeHint = ui::label(modeHint(m_drive.mode), "faint", p);
    m_modeHint->setWordWrap(true);
    mode->addWidget(m_modeHint);

    QVBoxLayout *ports;
    v->addWidget(section(p, tr("Motors"), &ports, tr("Which NXT output drives each side of the robot.")));
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);
    auto *left = motorCombo(p, false, m_drive.leftMotor);
    auto *right = motorCombo(p, false, m_drive.rightMotor);
    auto *aux = motorCombo(p, true, m_drive.auxMotor.value_or(-1));
    grid->addWidget(ui::label(tr("Left side"), "muted", p), 0, 0);
    grid->addWidget(ui::label(tr("Right side"), "muted", p), 0, 1);
    grid->addWidget(ui::label(tr("Extra (LB/RB)"), "muted", p), 0, 2);
    grid->addWidget(left, 1, 0);
    grid->addWidget(right, 1, 1);
    grid->addWidget(aux, 1, 2);
    const auto onPorts = [this, left, right, aux] {
        m_drive.leftMotor = left->currentData().toInt();
        m_drive.rightMotor = right->currentData().toInt();
        const int a = aux->currentData().toInt();
        m_drive.auxMotor = a < 0 ? std::nullopt : std::optional<int>(a);
        saveDrive();
    };
    for (QComboBox *cb : {left, right, aux}) connect(cb, &QComboBox::activated, this, onPorts);
    const auto reverse = [this, p](bool *field) {
        auto *t = new ui::ToggleSwitch(tr("Reverse"), p);
        t->setChecked(*field);
        connect(t, &ui::ToggleSwitch::toggled, this, [this, field](bool on) {
            *field = on;
            saveDrive();
        });
        return t;
    };
    grid->addWidget(reverse(&m_drive.invertLeft), 2, 0);
    grid->addWidget(reverse(&m_drive.invertRight), 2, 1);
    grid->addWidget(reverse(&m_drive.invertAux), 2, 2);
    ports->addLayout(grid);

    QVBoxLayout *feel;
    v->addWidget(section(p, tr("Feel"), &feel));
    const auto pct = [](int x) { return QStringLiteral("%1%").arg(x); };
    const auto frac = [](int x) { return QString::number(x / 100.0, 'f', 2); };
    feel->addWidget(slider(p, tr("Top speed"), 10, 100, m_drive.maxPower, pct, [this](int x) { m_drive.maxPower = x; saveDrive(); }));
    feel->addWidget(slider(p, tr("Extra motor power"), 10, 100, m_drive.auxPower, pct, [this](int x) { m_drive.auxPower = x; saveDrive(); }));
    feel->addWidget(slider(p, tr("Stick deadzone"), 0, 40, int(m_drive.deadzone * 100), frac,
                           [this](int x) { m_drive.deadzone = x / 100.0; saveDrive(); }));
    feel->addWidget(slider(p, tr("Fine control (expo)"), 0, 100, int(m_drive.expo * 100), frac,
                           [this](int x) { m_drive.expo = x / 100.0; saveDrive(); }));

    QVBoxLayout *motor;
    v->addWidget(section(p, tr("Motor behaviour"), &motor));
    auto &brick = nxt::Brick::instance();
    motor->addLayout(row(p, tr("Brake when stopped"), toggle(p, brick.brakeOnStop(), [](bool on) { nxt::Brick::instance().setBrakeOnStop(on); }),
                         tr("Holds the wheels in place instead of rolling to a stop.")));
    motor->addLayout(row(p, tr("Speed regulation"), toggle(p, brick.speedRegulation(), [](bool on) { nxt::Brick::instance().setSpeedRegulation(on); }),
                         tr("The NXT adds power on slopes and under load to keep the speed steady.")));
    v->addStretch();
    return p;
}

QWidget *SettingsDialog::buildBrick()
{
    auto &prefs = Prefs::instance();
    QVBoxLayout *v;
    QWidget *p = page(this, &v);
    QVBoxLayout *conn;
    v->addWidget(section(p, tr("Connection"), &conn));
    conn->addLayout(row(p, tr("Connect automatically"), toggle(p, prefs.autoConnect(), [](bool on) { Prefs::instance().setAutoConnect(on); }),
                        tr("Connects as soon as an NXT is plugged in.")));
    conn->addLayout(row(p, tr("Hello beep"), toggle(p, prefs.beepOnConnect(), [](bool on) { Prefs::instance().setBeepOnConnect(on); }),
                        tr("The brick plays a short tune when it connects.")));

    QVBoxLayout *reset;
    v->addWidget(section(p, tr("Factory reset"), &reset,
                         tr("Deletes the programs and files saved on the connected brick. The firmware stays; you see the list before anything is deleted.")));
    auto *resetBrick = ui::button(tr("Reset the brick…"), "danger", "reset", p);
    connect(resetBrick, &QPushButton::clicked, this, [this] {
        FactoryResetDialog dlg(this);
        theme::styleTitleBar(&dlg);
        connect(&dlg, &FactoryResetDialog::message, this, &SettingsDialog::message);
        dlg.exec();
    });
    reset->addWidget(resetBrick, 0, Qt::AlignLeft);
    v->addStretch();
    return p;
}

QWidget *SettingsDialog::buildAbout()
{
    QVBoxLayout *v;
    QWidget *p = page(this, &v);
    QVBoxLayout *about;
    v->addWidget(section(p, QStringLiteral("NXT Studio"), &about));
    about->addWidget(ui::label(tr("Version %1").arg(QStringLiteral(NXTSTUDIO_VERSION)), "muted", p));
    auto *desc = ui::label(tr("Block programming and live gamepad driving for LEGO MINDSTORMS NXT."), "muted", p);
    desc->setWordWrap(true);
    about->addWidget(desc);
    auto *github = ui::button(tr("Project on GitHub"), "ghost", "star", p);
    connect(github, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/xee1a/mindstorms"))); });
    about->addWidget(github, 0, Qt::AlignLeft);

    QVBoxLayout *reset;
    v->addWidget(section(p, tr("Reset"), &reset, tr("Puts every setting back to how it was on first launch. Your programs are not touched.")));
    auto *resetButton = ui::button(tr("Reset all settings"), "danger", "reset", p);
    connect(resetButton, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Reset settings"), tr("Reset all settings and restart NXT Studio?")) != QMessageBox::Yes) return;
        Prefs::instance().resetAll();
        accept();
        emit restartRequested();
    });
    reset->addWidget(resetButton, 0, Qt::AlignLeft);
    v->addStretch();
    return p;
}
