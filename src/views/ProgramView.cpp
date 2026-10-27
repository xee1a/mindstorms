#include "ProgramView.h"

#include "app/Widgets.h"
#include "blocks/Canvas.h"
#include "blocks/Palette.h"
#include "blocks/Workspace.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QUndoStack>
#include <QVBoxLayout>

namespace {

constexpr char FileFormat[] = "nxt-studio";
constexpr int FileVersion = 2;

/** Undo step that swaps whole-workspace snapshots; simple and always consistent. */
class SnapshotCommand : public QUndoCommand {
public:
    SnapshotCommand(std::function<void(const QJsonObject &)> apply, QJsonObject before, QJsonObject after)
        : m_apply(std::move(apply)), m_before(std::move(before)), m_after(std::move(after))
    {
    }
    void undo() override { m_apply(m_before); }
    void redo() override
    {
        // The edit already happened when the command was pushed.
        if (m_skipFirst) {
            m_skipFirst = false;
            return;
        }
        m_apply(m_after);
    }

private:
    std::function<void(const QJsonObject &)> m_apply;
    QJsonObject m_before;
    QJsonObject m_after;
    bool m_skipFirst = true;
};

QString autosavePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/autosave.nxtb");
}

} // namespace

ProgramView::ProgramView(QWidget *parent) : QWidget(parent)
{
    m_ws = new blocks::Workspace(this);
    m_undo = new QUndoStack(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("toolbar"));
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(54);
    auto *tb = new QHBoxLayout(bar);
    tb->setContentsMargins(14, 0, 14, 0);
    tb->setSpacing(6);
    m_name = new QLineEdit(bar);
    m_name->setObjectName(QStringLiteral("projectName"));
    m_name->setFixedWidth(240);
    m_count = ui::label({}, "faint", bar);
    tb->addWidget(m_name);
    tb->addWidget(m_count);
    tb->addStretch();

    auto *undo = ui::button({}, "ghost", "undo", bar);
    undo->setToolTip(tr("Undo (Ctrl+Z)"));
    auto *redo = ui::button({}, "ghost", "redo", bar);
    redo->setToolTip(tr("Redo (Ctrl+Y)"));
    auto *newBtn = ui::button(tr("New"), "ghost", "plus", bar);
    auto *openBtn = ui::button(tr("Open"), "ghost", "folder", bar);
    auto *saveBtn = ui::button(tr("Save"), "ghost", "save", bar);
    auto *divider = new QFrame(bar);
    divider->setFrameShape(QFrame::VLine);
    divider->setStyleSheet(QStringLiteral("color:#2b3647"));
    auto *run = ui::button(tr("Download && run"), "primary", "play", bar);
    run->setEnabled(false);
    run->setToolTip(tr("Compiling and downloading to the NXT arrives in the next milestone"));
    for (QWidget *w : {static_cast<QWidget *>(undo), static_cast<QWidget *>(redo), static_cast<QWidget *>(newBtn),
                       static_cast<QWidget *>(openBtn), static_cast<QWidget *>(saveBtn), static_cast<QWidget *>(divider),
                       static_cast<QWidget *>(run)})
        tb->addWidget(w);
    root->addWidget(bar);

    // Palette + canvas
    auto *body = new QHBoxLayout;
    body->setSpacing(0);
    m_palette = new blocks::Palette(this);
    m_canvas = new blocks::Canvas(m_ws, this);
    body->addWidget(m_palette);
    body->addWidget(m_canvas, 1);
    root->addLayout(body, 1);

    // Dropping a block back on the palette deletes it.
    m_ws->isOverDeleteArea = [this](const QPoint &global) { return m_palette->rect().contains(m_palette->mapFromGlobal(global)); };

    connect(m_ws, &blocks::Workspace::changed, this, &ProgramView::onChanged);
    connect(m_ws, &blocks::Workspace::variablesChanged, this, [this] { m_palette->rebuild(m_ws->variables()); });
    connect(m_palette, &blocks::Palette::makeVariableClicked, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New variable"), tr("Variable name:"), QLineEdit::Normal, {}, &ok);
        if (ok && !m_ws->addVariable(name) && !name.trimmed().isEmpty())
            emit message(tr("There is already a variable called “%1”.").arg(name.trimmed()), "error");
    });
    connect(m_canvas, &blocks::Canvas::undoRequested, m_undo, &QUndoStack::undo);
    connect(m_canvas, &blocks::Canvas::redoRequested, m_undo, &QUndoStack::redo);
    connect(undo, &QPushButton::clicked, m_undo, &QUndoStack::undo);
    connect(redo, &QPushButton::clicked, m_undo, &QUndoStack::redo);
    connect(m_undo, &QUndoStack::canUndoChanged, undo, &QWidget::setEnabled);
    connect(m_undo, &QUndoStack::canRedoChanged, redo, &QWidget::setEnabled);
    undo->setEnabled(false);
    redo->setEnabled(false);
    connect(newBtn, &QPushButton::clicked, this, &ProgramView::newProject);
    connect(openBtn, &QPushButton::clicked, this, &ProgramView::openProject);
    connect(saveBtn, &QPushButton::clicked, this, &ProgramView::saveProject);
    connect(m_name, &QLineEdit::textEdited, this, [this] { m_autosave.start(); });

    const auto shortcut = [this](const QKeySequence &k, auto fn) {
        auto *s = new QShortcut(k, this);
        s->setContext(Qt::WidgetWithChildrenShortcut);
        connect(s, &QShortcut::activated, this, fn);
    };
    shortcut(QKeySequence::Undo, [this] { m_undo->undo(); });
    shortcut(QKeySequence::Redo, [this] { m_undo->redo(); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_Y), [this] { m_undo->redo(); });
    shortcut(QKeySequence::Save, [this] { saveProject(); });
    shortcut(QKeySequence::Open, [this] { openProject(); });

    m_autosave.setSingleShot(true);
    m_autosave.setInterval(500);
    connect(&m_autosave, &QTimer::timeout, this, &ProgramView::autosave);

    // Restore the last session, or start with an example.
    QJsonObject restored;
    QFile f(autosavePath());
    if (f.open(QIODevice::ReadOnly)) restored = QJsonDocument::fromJson(f.readAll()).object();
    if (restored.value(QStringLiteral("format")).toString() == QLatin1String(FileFormat)) {
        m_name->setText(restored.value(QStringLiteral("name")).toString());
        load(restored.value(QStringLiteral("workspace")).toObject(), true);
    } else {
        m_name->setText(tr("My robot"));
        load(starter(), true);
    }
}

QJsonObject ProgramView::starter() const
{
    const auto block = [](const char *type, QJsonObject fields = {}, QJsonObject extra = {}) {
        QJsonObject o{{QStringLiteral("type"), QLatin1String(type)}};
        if (!fields.isEmpty()) o[QStringLiteral("fields")] = fields;
        for (auto it = extra.begin(); it != extra.end(); ++it) o[it.key()] = it.value();
        return o;
    };
    QJsonObject tone = block("nxt_play_tone", {{"FREQ", "880"}, {"DURATION", "200"}});
    QJsonObject stop = block("nxt_motor_stop", {{"PORT", "ABC"}, {"MODE", "BRAKE"}}, {{"next", tone}});
    QJsonObject wait = block("nxt_wait_touch", {{"PORT", "1"}, {"STATE", "PRESSED"}}, {{"next", stop}});
    QJsonObject move = block("nxt_move_steer", {{"LEFT", "B"}, {"RIGHT", "C"}, {"STEER", "0"}, {"POWER", "75"}}, {{"next", wait}});
    QJsonObject start = block("nxt_when_start", {}, {{"next", move}});
    return QJsonObject{
        {QStringLiteral("variables"), QJsonArray{tr("counter")}},
        {QStringLiteral("stacks"), QJsonArray{QJsonObject{{"x", 40}, {"y", 40}, {"block", start}}}},
    };
}

void ProgramView::load(const QJsonObject &workspace, bool resetHistory)
{
    m_restoring = true;
    m_ws->loadJson(workspace);
    m_restoring = false;
    m_snapshot = m_ws->toJson();
    m_palette->rebuild(m_ws->variables());
    if (resetHistory) m_undo->clear();
    updateCount();
    m_canvas->scrollToContent();
}

void ProgramView::onChanged()
{
    if (m_restoring) return;
    const QJsonObject after = m_ws->toJson();
    if (after == m_snapshot) return;
    m_undo->push(new SnapshotCommand(
        [this](const QJsonObject &state) {
            m_restoring = true;
            m_ws->loadJson(state);
            m_restoring = false;
            m_snapshot = state;
            updateCount();
            m_autosave.start();
        },
        m_snapshot, after));
    m_snapshot = after;
    updateCount();
    m_autosave.start();
}

void ProgramView::updateCount()
{
    const int n = m_ws->blockCount();
    m_count->setText(tr("%n block(s)", nullptr, n));
}

void ProgramView::autosave()
{
    const QJsonObject doc{{"format", FileFormat}, {"version", FileVersion}, {"name", m_name->text()}, {"workspace", m_snapshot}};
    QFile f(autosavePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument(doc).toJson(QJsonDocument::Compact));
}

void ProgramView::newProject()
{
    if (m_ws->blockCount() > 1 &&
        QMessageBox::question(this, tr("New project"), tr("Start a new project? Blocks that aren't saved to a file will be lost.")) !=
            QMessageBox::Yes)
        return;
    m_name->setText(tr("My robot"));
    load(starter(), true);
    autosave();
}

void ProgramView::openProject()
{
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open project"), settings.value("lastDir").toString(),
                                                      tr("NXT Studio projects (*.nxtb)"));
    if (path.isEmpty()) return;
    settings.setValue("lastDir", QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit message(tr("Couldn't open %1.").arg(QFileInfo(path).fileName()), "error");
        return;
    }
    const QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
    if (doc.value(QStringLiteral("format")).toString() != QLatin1String(FileFormat) || doc.value("version").toInt() > FileVersion) {
        emit message(tr("%1 isn't an NXT Studio project.").arg(QFileInfo(path).fileName()), "error");
        return;
    }
    m_name->setText(doc.value(QStringLiteral("name")).toString(QFileInfo(path).completeBaseName()));
    load(doc.value(QStringLiteral("workspace")).toObject(), true);
    autosave();
    emit message(tr("Opened “%1”").arg(m_name->text()), "success");
}

void ProgramView::saveProject()
{
    QSettings settings;
    QString suggested = m_name->text().trimmed();
    suggested.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]+")), QStringLiteral("_"));
    if (suggested.isEmpty()) suggested = QStringLiteral("program");
    const QString path = QFileDialog::getSaveFileName(this, tr("Save project"),
                                                      settings.value("lastDir").toString() + QLatin1Char('/') + suggested + ".nxtb",
                                                      tr("NXT Studio projects (*.nxtb)"));
    if (path.isEmpty()) return;
    settings.setValue("lastDir", QFileInfo(path).absolutePath());
    const QJsonObject doc{{"format", FileFormat}, {"version", FileVersion}, {"name", m_name->text()}, {"workspace", m_ws->toJson()}};
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(QJsonDocument(doc).toJson()) < 0) {
        emit message(tr("Couldn't save %1.").arg(QFileInfo(path).fileName()), "error");
        return;
    }
    emit message(tr("Saved “%1”").arg(QFileInfo(path).fileName()), "success");
}

void ProgramView::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (m_firstShow) {
        m_firstShow = false;
        QTimer::singleShot(0, m_canvas, &blocks::Canvas::scrollToContent);
    }
}
