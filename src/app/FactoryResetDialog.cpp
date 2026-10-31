#include "FactoryResetDialog.h"

#include "Theme.h"
#include "nxt/SystemFiles.h"
#include "Widgets.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

/** What a reset with these options deletes. The brick's settings (*.sys) always stay. */
bool removes(const nxt::BrickFile &f, bool everything)
{
    if (f.name.endsWith(QLatin1String(".sys"), Qt::CaseInsensitive)) return false;
    return everything || !f.builtIn();
}

} // namespace

FactoryResetDialog::FactoryResetDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Factory reset"));
    setMinimumWidth(500);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(22, 20, 22, 18);
    v->setSpacing(10);
    v->addWidget(ui::label(tr("Reset the brick to factory settings"), "h2", this));

    auto *what = ui::label(tr("<p><b>Removed:</b> your programs, pictures and data files.</p>"
                              "<p><b>Kept:</b> the firmware, the brick's own settings and, unless you tick the box below, "
                              "sounds and LEGO's Try Me programs.</p>"
                              "<p><b>Put back:</b> sounds and Try Me programs that this PC has seen on a brick before, "
                              "if they are missing.</p>"
                              "<p>Deleted programs can't be brought back.</p>"),
                           "muted", this);
    what->setWordWrap(true);
    v->addWidget(what);

    m_summary = ui::label(tr("Reading the files on the brick…"), nullptr, this);
    m_summary->setWordWrap(true);
    v->addWidget(m_summary);
    m_list = new QListWidget(this);
    m_list->setMinimumHeight(140);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    v->addWidget(m_list);

    m_everything = new QCheckBox(tr("Also remove sounds and LEGO's Try Me programs (frees the most space)"), this);
    m_rename = new QCheckBox(tr("Rename the brick back to “NXT”"), this);
    m_rename->setChecked(true);
    m_confirm = new QCheckBox(tr("I understand that the files listed above will be deleted"), this);
    v->addWidget(m_everything);
    v->addWidget(m_rename);
    v->addWidget(m_confirm);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(8);
    m_progress->hide();
    v->addWidget(m_progress);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    m_cancel = ui::button(tr("Cancel"), "ghost", nullptr, this);
    m_reset = ui::button(tr("Delete and reset"), "danger", "reset", this);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_reset);
    v->addSpacing(6);
    v->addLayout(buttons);

    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_reset, &QPushButton::clicked, this, &FactoryResetDialog::start);
    connect(m_everything, &QCheckBox::toggled, this, &FactoryResetDialog::updateSummary);
    connect(m_rename, &QCheckBox::toggled, this, &FactoryResetDialog::updateSummary);
    connect(m_confirm, &QCheckBox::toggled, this, &FactoryResetDialog::updateSummary);

    auto &brick = nxt::Brick::instance();
    connect(&brick, &nxt::Brick::filesListed, this, &FactoryResetDialog::listed);
    m_doneConn = connect(&brick, &nxt::Brick::jobFinished, this, [this](const QString &error) {
        disconnect(m_doneConn);
        m_loading = false;
        if (!error.isEmpty()) m_summary->setText(tr("Couldn't read the files: %1").arg(error));
        updateSummary();
    });
    if (!brick.connected() || !brick.listFiles()) {
        disconnect(m_doneConn);
        m_loading = false;
        m_summary->setText(brick.connected() ? tr("The brick is busy with another task. Close this window and try again.")
                                             : tr("Connect the NXT first (Brick tab)."));
    }
    updateSummary();
}

void FactoryResetDialog::reject()
{
    if (!m_working) QDialog::reject();
}

void FactoryResetDialog::listed(const QList<nxt::BrickFile> &files)
{
    m_files = files;
    std::sort(m_files.begin(), m_files.end(), [](const auto &a, const auto &b) { return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
}

void FactoryResetDialog::updateSummary()
{
    const bool everything = m_everything->isChecked();
    m_list->clear();
    int count = 0;
    quint64 bytes = 0;
    for (const nxt::BrickFile &f : m_files) {
        const bool gone = removes(f, everything);
        auto *item = new QListWidgetItem(QStringLiteral("%1   ·   %2 KB   ·   %3")
                                             .arg(f.name)
                                             .arg(f.size / 1024.0, 0, 'f', 1)
                                             .arg(gone ? tr("will be deleted") : tr("stays")),
                                         m_list);
        item->setForeground(gone ? theme::DangerText : theme::Text3);
        if (gone) {
            ++count;
            bytes += f.size;
        }
    }
    // Missing sounds and Try Me programs get written back (from the app's copies, or its own system sounds).
    int restored = 0;
    if (!everything && !m_files.isEmpty()) {
        for (const QString &name : nxt::systemfiles::available()) {
            const bool present = std::any_of(m_files.begin(), m_files.end(), [&](const auto &f) { return f.name.compare(name, Qt::CaseInsensitive) == 0; });
            if (present) continue;
            auto *item = new QListWidgetItem(QStringLiteral("%1   ·   %2").arg(name, tr("will be put back")), m_list);
            item->setForeground(theme::Good);
            ++restored;
        }
    }
    if (!m_loading && !m_files.isEmpty()) {
        QString text = count == 0 ? tr("There's nothing to delete: the brick only holds sounds and LEGO's own files.")
                                  : tr("%n file(s) will be deleted, freeing %1 KB.", nullptr, count).arg(bytes / 1024.0, 0, 'f', 1);
        if (restored > 0) text += ' ' + tr("%n missing file(s) will be put back if there's room.", nullptr, restored);
        m_summary->setText(text);
    }
    const bool canReset = !m_loading && !m_working && nxt::Brick::instance().connected() && (count > 0 || restored > 0 || m_rename->isChecked()) && !m_files.isEmpty();
    m_confirm->setEnabled(canReset);
    m_reset->setEnabled(canReset && m_confirm->isChecked());
}

void FactoryResetDialog::start()
{
    auto &brick = nxt::Brick::instance();
    m_working = true;
    m_progress->setValue(0);
    m_progress->show();
    m_cancel->setEnabled(false);
    m_everything->setEnabled(false);
    m_rename->setEnabled(false);
    m_summary->setText(tr("Resetting the brick… Don't switch it off."));
    m_progressConn = connect(&brick, &nxt::Brick::jobProgress, m_progress, &QProgressBar::setValue);
    m_doneConn = connect(&brick, &nxt::Brick::jobFinished, this, &FactoryResetDialog::finished);
    if (!brick.factoryReset(m_everything->isChecked(), m_rename->isChecked())) finished(tr("The brick is busy with another task. Try again in a moment."));
    updateSummary();
}

void FactoryResetDialog::finished(const QString &error)
{
    disconnect(m_progressConn);
    disconnect(m_doneConn);
    m_working = false;
    m_cancel->setEnabled(true);
    if (!error.isEmpty()) {
        m_summary->setText(tr("The reset stopped: %1").arg(error));
        m_progress->hide();
        updateSummary();
        return;
    }
    emit message(tr("The brick is back to factory settings"), "success");
    accept();
}
