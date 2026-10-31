#include "MainWindow.h"

#include "Prefs.h"
#include "SettingsDialog.h"
#include "Theme.h"
#include "Widgets.h"
#include "compile/Compiler.h"
#include "compile/NxcGenerator.h"
#include "drive/Gamepad.h"
#include "drive/PadLink.h"
#include "views/BrickView.h"
#include "views/ControllerView.h"
#include "views/ProgramView.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QThread>
#include <QSvgWidget>
#include <QToolButton>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("NXT Studio"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    setMinimumSize(1100, 700);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto *v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(buildHeader());

    m_stack = new QStackedWidget(central);
    auto *program = new ProgramView(m_stack);
    auto *controller = new ControllerView(m_stack);
    auto *brick = new BrickView(m_stack);
    m_stack->addWidget(program);
    m_stack->addWidget(controller);
    m_stack->addWidget(brick);
    v->addWidget(m_stack, 1);
    setCentralWidget(central);

    m_toast = new ui::Toast(central);
    const auto toast = [this](const QString &text, const char *kind) { m_toast->showMessage(text, kind); };
    connect(program, &ProgramView::message, this, toast);
    connect(brick, &BrickView::message, this, toast);

    connect(&drive::Gamepad::instance(), &drive::Gamepad::connectionChanged, this, [this](bool on) {
        m_padConnected = on;
        updatePadChip();
    });
    // Programs on the brick that use gamepad blocks read the pad from an inbox; the worker only sends while one runs.
    connect(&drive::Gamepad::instance(), &drive::Gamepad::updated, this, [](const drive::PadSnapshot &snap) {
        if (!snap.connected) return nxt::Brick::instance().setPadMessage({});
        uint16_t bits = 0;
        for (int i = 0; i < int(drive::PadButton::Count); ++i)
            if (snap.buttons[i]) bits |= uint16_t(1u << i);
        nxt::Brick::instance().setPadMessage(drive::encodePadMessage(drive::padAxes(snap.input), bits));
    });
    auto &nxtBrick = nxt::Brick::instance();
    connect(&nxtBrick, &nxt::Brick::stateChanged, this, &MainWindow::onBrick);
    onBrick(nxtBrick.state());
    nxtBrick.setAutoConnect(Prefs::instance().autoConnect());
    connect(&Prefs::instance(), &Prefs::changed, this, [] { nxt::Brick::instance().setAutoConnect(Prefs::instance().autoConnect()); });

    // After a Bluetooth connection the brick briefly shows this on its screen, in the app's language.
    const QString line1 = compile::toNxtText(tr("Connected", "on the brick's screen, line 1"));
    const QString line2 = compile::toNxtText(tr("over Bluetooth", "on the brick's screen, line 2"));
    QThread *greeting = QThread::create([line1, line2] {
        const auto x = [](const QString &t) { return QString::number(std::max(0, (100 - 6 * int(t.size())) / 2)); };
        const auto quoted = [](QString t) { return QLatin1Char('"') + t.remove(QLatin1Char('"')).remove(QLatin1Char('\\')).left(16) + QLatin1Char('"'); };
        const QString source = QStringLiteral("task main()\n{\n    ClearScreen();\n    TextOut(%1, LCD_LINE3, %2);\n"
                                              "    TextOut(%3, LCD_LINE5, %4);\n    Wait(2500);\n}\n")
                                   .arg(x(line1), quoted(line1), x(line2), quoted(line2));
        const compile::CompileResult r = compile::compileSource(source);
        if (!r.rxe.isEmpty()) nxt::Brick::instance().setBluetoothGreeting(r.rxe);
    });
    connect(greeting, &QThread::finished, greeting, &QObject::deleteLater);
    greeting->start();

    QSettings s;
    restoreGeometry(s.value(QStringLiteral("window/geometry")).toByteArray());
    const int start = Prefs::instance().startTab();
    showView(start >= 0 ? start : std::clamp(s.value(QStringLiteral("window/view"), 0).toInt(), 0, 2));
}

QWidget *MainWindow::buildHeader()
{
    auto *h = new QWidget(this);
    h->setObjectName(QStringLiteral("header"));
    h->setAttribute(Qt::WA_StyledBackground);
    h->setFixedHeight(52);
    auto *l = new QHBoxLayout(h);
    l->setContentsMargins(16, 0, 12, 0);
    l->setSpacing(10);

    auto *logo = new QSvgWidget(QStringLiteral(":/icons/app.svg"), h);
    logo->setFixedSize(24, 24);
    l->addWidget(logo);
    auto *name = new QLabel(QStringLiteral("NXT Studio"), h);
    name->setObjectName(QStringLiteral("brandName"));
    l->addWidget(name);
    l->addStretch(1);

    auto *tabs = new QFrame(h);
    tabs->setObjectName(QStringLiteral("tabs"));
    auto *tl = new QHBoxLayout(tabs);
    tl->setContentsMargins(3, 3, 3, 3);
    tl->setSpacing(2);
    m_tabs = new QButtonGroup(this);
    const std::pair<const char *, QString> items[] = {{"blocks", tr("Program")}, {"gamepad", tr("Controller")}, {"brick", tr("Brick")}};
    int id = 0;
    for (const auto &[icon, text] : items) {
        auto *b = new QPushButton(text, tabs);
        b->setObjectName(QStringLiteral("tab"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setIcon(ui::icon(icon));
        b->setIconSize(QSize(16, 16));
        m_tabs->addButton(b, id++);
        tl->addWidget(b);
    }
    connect(m_tabs, &QButtonGroup::idClicked, this, &MainWindow::showView);
    // The tabs float over the header so they stay centred on the window, whatever sits left and right.
    m_tabBar = tabs;
    h->installEventFilter(this);

    m_padChip = new QLabel(tr("●  No gamepad"), h);
    m_padChip->setObjectName(QStringLiteral("chip"));
    m_padChip->setFixedHeight(28);
    l->addWidget(m_padChip);
    m_brickChip = new QPushButton(h);
    m_brickChip->setObjectName(QStringLiteral("chip"));
    m_brickChip->setCursor(Qt::PointingHandCursor);
    m_brickChip->setFixedHeight(28);
    connect(m_brickChip, &QPushButton::clicked, this, [this] { showView(2); });
    l->addWidget(m_brickChip);

    auto *settings = new QToolButton(h);
    settings->setObjectName(QStringLiteral("settingsButton"));
    settings->setIcon(ui::icon("gear"));
    settings->setIconSize(QSize(18, 18));
    settings->setToolTip(tr("Settings"));
    settings->setCursor(Qt::PointingHandCursor);
    connect(settings, &QToolButton::clicked, this, &MainWindow::openSettings);
    l->addWidget(settings);
    return h;
}

bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Resize && m_tabBar && o == m_tabBar->parentWidget()) {
        const QSize hint = m_tabBar->sizeHint();
        auto *header = m_tabBar->parentWidget();
        m_tabBar->setGeometry((header->width() - hint.width()) / 2, (header->height() - hint.height()) / 2, hint.width(), hint.height());
    }
    return QMainWindow::eventFilter(o, e);
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(this);
    theme::styleTitleBar(&dlg);
    connect(&dlg, &SettingsDialog::restartRequested, this, &MainWindow::restart, Qt::QueuedConnection);
    connect(&dlg, &SettingsDialog::message, this, [this](const QString &text, const char *kind) { m_toast->showMessage(text, kind); });
    dlg.exec();
}

void MainWindow::showView(int index)
{
    m_stack->setCurrentIndex(index);
    if (auto *b = m_tabs->button(index)) b->setChecked(true);
    QSettings().setValue(QStringLiteral("window/view"), index);
}

void MainWindow::onBrick(const nxt::BrickState &s)
{
    QString text;
    m_brickChip->setIcon({});
    m_brickChip->setToolTip({});
    if (s.status == nxt::LinkStatus::Connected) {
        text = QStringLiteral("●  %1").arg(s.name);
        if (s.batteryMv) {
            const nxt::BatteryLevel level = nxt::batteryLevel(*s.batteryMv);
            m_brickChip->setIcon(ui::BatteryWidget::icon(level));
            m_brickChip->setIconSize(QSize(22, 22));
            m_brickChip->setToolTip(QStringLiteral("%1 · %2").arg(nxt::batteryLabel(level), s.link));
        }
    } else if (s.status == nxt::LinkStatus::Connecting) {
        text = tr("●  Connecting…");
    } else {
        text = tr("●  Not connected");
    }
    m_brickChip->setText(text);
    if (s.status == nxt::LinkStatus::Connected && m_lastStatus != nxt::LinkStatus::Connected && Prefs::instance().beepOnConnect()) {
        nxt::Brick::instance().playTone(523, 90);
        nxt::Brick::instance().playTone(784, 140);
    }
    m_lastStatus = s.status;
    if (s.padStreaming != m_padStreaming) {
        m_padStreaming = s.padStreaming;
        updatePadChip();
    }
    m_brickChip->setProperty("ok", s.status == nxt::LinkStatus::Connected);
    theme::repolish(m_brickChip);

    if (s.errorSerial != m_lastErrorSerial) {
        m_lastErrorSerial = s.errorSerial;
        if (!s.lastError.isEmpty()) m_toast->showMessage(s.lastError, "error");
    }
}

void MainWindow::updatePadChip()
{
    if (m_padStreaming) m_padChip->setText(tr("●  Gamepad → NXT"));
    else m_padChip->setText(m_padConnected ? tr("●  Gamepad") : tr("●  No gamepad"));
    m_padChip->setToolTip(m_padStreaming ? tr("The program on the brick is reading this gamepad.") : QString());
    m_padChip->setProperty("ok", m_padConnected);
    theme::repolish(m_padChip);
}

void MainWindow::restart()
{
    close();
    QProcess::startDetached(QApplication::applicationFilePath(), {});
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    QSettings().setValue(QStringLiteral("window/geometry"), saveGeometry());
    QMainWindow::closeEvent(e);
}
