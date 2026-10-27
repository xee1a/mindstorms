#include "MainWindow.h"

#include "Theme.h"
#include "Widgets.h"
#include "drive/Gamepad.h"
#include "views/BrickView.h"
#include "views/DriveView.h"
#include "views/ProgramView.h"

#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
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
    auto *drive = new DriveView(m_stack);
    auto *brick = new BrickView(m_stack);
    m_stack->addWidget(program);
    m_stack->addWidget(drive);
    m_stack->addWidget(brick);
    v->addWidget(m_stack, 1);
    setCentralWidget(central);

    m_toast = new ui::Toast(central);
    const auto toast = [this](const QString &text, const char *kind) { m_toast->showMessage(text, kind); };
    connect(program, &ProgramView::message, this, toast);
    connect(drive, &DriveView::message, this, toast);
    connect(brick, &BrickView::message, this, toast);

    connect(&drive::Gamepad::instance(), &drive::Gamepad::connectionChanged, this, [this](bool on) {
        m_padChip->setText(on ? tr("●  Controller") : tr("●  No controller"));
        m_padChip->setProperty("ok", on);
        theme::repolish(m_padChip);
    });
    connect(&nxt::Brick::instance(), &nxt::Brick::stateChanged, this, &MainWindow::onBrick);
    onBrick(nxt::Brick::instance().state());

    QSettings s;
    restoreGeometry(s.value(QStringLiteral("window/geometry")).toByteArray());
    showView(std::clamp(s.value(QStringLiteral("window/view"), 0).toInt(), 0, 2));
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
    const std::pair<const char *, QString> items[] = {{"blocks", tr("Program")}, {"gamepad", tr("Drive")}, {"brick", tr("Brick")}};
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
    l->addWidget(tabs);
    l->addStretch(1);

    m_padChip = new QLabel(tr("●  No controller"), h);
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
    settings->setPopupMode(QToolButton::InstantPopup);
    settings->setCursor(Qt::PointingHandCursor);
    auto *menu = new QMenu(settings);
    auto *lang = menu->addMenu(tr("Language"));
    auto *group = new QActionGroup(lang);
    const QString current = QSettings().value(QStringLiteral("language"), QStringLiteral("pl")).toString();
    for (const auto &[code, label] : {std::pair{QStringLiteral("pl"), QStringLiteral("Polski")}, std::pair{QStringLiteral("en"), QStringLiteral("English")}}) {
        QAction *a = lang->addAction(label);
        a->setCheckable(true);
        a->setChecked(code == current);
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, code] { switchLanguage(code); });
    }
    settings->setMenu(menu);
    l->addWidget(settings);
    return h;
}

void MainWindow::showView(int index)
{    m_stack->setCurrentIndex(index);
    if (auto *b = m_tabs->button(index)) b->setChecked(true);
    QSettings().setValue(QStringLiteral("window/view"), index);
}

void MainWindow::onBrick(const nxt::BrickState &s)
{
    QString text;
    if (s.status == nxt::LinkStatus::Connected) {
        text = s.batteryMv ? QStringLiteral("●  %1 · %2%").arg(s.name).arg(nxt::batteryPercent(*s.batteryMv)) : QStringLiteral("●  %1").arg(s.name);
    } else if (s.status == nxt::LinkStatus::Connecting) {
        text = tr("●  Connecting…");
    } else {
        text = tr("●  Not connected");
    }
    m_brickChip->setText(text);
    m_brickChip->setProperty("ok", s.status == nxt::LinkStatus::Connected);
    theme::repolish(m_brickChip);

    if (s.errorSerial != m_lastErrorSerial) {
        m_lastErrorSerial = s.errorSerial;
        if (!s.lastError.isEmpty()) m_toast->showMessage(s.lastError, "error");
    }
}

void MainWindow::switchLanguage(const QString &code)
{
    QSettings s;
    if (s.value(QStringLiteral("language"), QStringLiteral("pl")).toString() == code) return;
    s.setValue(QStringLiteral("language"), code);
    const auto answer = QMessageBox::question(this, tr("Change language"),
                                              tr("NXT Studio needs to restart to change the language. Restart now?"));
    if (answer != QMessageBox::Yes) return;
    close();
    QProcess::startDetached(QApplication::applicationFilePath(), {});
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    QSettings().setValue(QStringLiteral("window/geometry"), saveGeometry());
    QMainWindow::closeEvent(e);
}
