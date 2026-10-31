#include "UploadDialog.h"

#include "app/Theme.h"
#include "app/Widgets.h"
#include "compile/Compiler.h"
#include "compile/NxcGenerator.h"
#include "nxt/Brick.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QThread>
#include <QVBoxLayout>

namespace {

/** The NXT keeps 15 characters before ".rxe". Spaces work on the brick but are awkward in its menu, so they become '_'. */
constexpr int MaxName = 15;

} // namespace

QString UploadDialog::suggestedName(const QString &projectName)
{
    QString n = compile::toNxtText(projectName).trimmed();
    n.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    n.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")));
    n.truncate(MaxName);
    return n.isEmpty() ? QStringLiteral("program") : n;
}

UploadDialog::UploadDialog(const QJsonObject &workspace, const QString &projectName, QWidget *parent)
    : QDialog(parent), m_workspace(workspace), m_projectName(projectName)
{
    setWindowTitle(tr("Upload to NXT"));
    setMinimumWidth(460);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(22, 20, 22, 18);
    v->setSpacing(10);

    v->addWidget(ui::label(tr("Upload to NXT"), "h2", this));
    auto *intro = ui::label(tr("The program is saved on the brick, so it runs without the cable: "
                               "on the NXT open <b>My Files → Software files</b> and pick it."),
                            "muted", this);
    intro->setWordWrap(true);
    v->addWidget(intro);

    v->addSpacing(4);
    v->addWidget(ui::label(tr("Name on the brick"), nullptr, this));
    auto *nameRow = new QHBoxLayout;
    m_name = new QLineEdit(suggestedName(projectName), this);
    m_name->setMaxLength(MaxName);
    m_name->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9_-]{1,15}")), m_name));
    nameRow->addWidget(m_name, 1);
    nameRow->addWidget(ui::label(QStringLiteral(".rxe"), "faint", this));
    v->addLayout(nameRow);
    v->addWidget(ui::label(tr("Up to 15 letters (without Polish characters), digits, - or _."), "faint", this));

    m_run = new QCheckBox(tr("Start the program right after uploading"), this);
    m_run->setChecked(true);
    v->addWidget(m_run);

    m_status = ui::label({}, "muted", this);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(8);
    m_error = ui::label({}, "warn", this);
    m_error->setWordWrap(true);
    v->addWidget(m_status);
    v->addWidget(m_progress);
    v->addWidget(m_error);
    m_status->hide();
    m_progress->hide();
    m_error->hide();

    auto *buttons = new QHBoxLayout;
    m_details = ui::button(tr("Details"), "ghost", nullptr, this);
    m_details->hide();
    buttons->addWidget(m_details);
    buttons->addStretch();
    m_cancel = ui::button(tr("Cancel"), "ghost", nullptr, this);
    m_upload = ui::button(tr("Upload"), "primary", "save", this);
    m_upload->setDefault(true);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_upload);
    v->addSpacing(6);
    v->addLayout(buttons);

    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_upload, &QPushButton::clicked, this, &UploadDialog::start);
    connect(m_name, &QLineEdit::textChanged, this, [this] { m_upload->setEnabled(!m_working && m_name->hasAcceptableInput()); });
    connect(m_details, &QPushButton::clicked, this, [this] {
        QMessageBox box(QMessageBox::Information, tr("Compiler details"),
                        tr("Please send these details to the author of NXT Studio, together with the project file."), QMessageBox::Close, this);
        box.setDetailedText(m_detailsText);
        box.exec();
    });
}

void UploadDialog::reject()
{
    // Closing mid-upload would leave half a file on the brick; the job finishes in a few seconds.
    if (!m_working) QDialog::reject();
}

void UploadDialog::setWorking(bool on, const QString &status)
{
    m_working = on;
    m_upload->setEnabled(!on && m_name->hasAcceptableInput());
    m_name->setEnabled(!on);
    m_run->setEnabled(!on);
    m_cancel->setEnabled(!on);
    m_status->setVisible(on);
    m_status->setText(status);
    m_progress->setVisible(on);
    if (on) {
        m_error->hide();
        m_details->hide();
    }
}

void UploadDialog::showError(const QString &text, const QString &details)
{
    setWorking(false);
    m_error->setText(text);
    m_error->show();
    m_detailsText = details;
    m_details->setVisible(!details.isEmpty());
    m_upload->setText(tr("Try again"));
}

void UploadDialog::start()
{
    if (m_working || !m_name->hasAcceptableInput()) return;
    if (!nxt::Brick::instance().connected()) return showError(tr("Connect the NXT first (USB cable or Bluetooth, on the Brick tab)."));
    if (nxt::Brick::instance().busy()) return showError(tr("The brick is busy with another task. Wait a moment and try again."));
    setWorking(true, tr("Preparing the program…"));
    m_progress->setRange(0, 0); // busy until the upload reports progress

    // Compiling takes a second or so; keep the window responsive meanwhile.
    QPointer<UploadDialog> self = this;
    QThread *worker = QThread::create([self, ws = m_workspace, name = m_projectName] {
        const compile::CompileResult r = compile::compileWorkspace(ws, name);
        // Details only matter when the compiler itself complained, which means a bug in the generated code.
        const QString details = r.log.isEmpty() ? QString() : r.log + QStringLiteral("\n\n--- NXC ---\n") + r.source;
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, rxe = r.rxe, error = r.error, details] {
            if (self) self->compiled(rxe, error, details);
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void UploadDialog::compiled(const QByteArray &rxe, const QString &error, const QString &details)
{
    if (!error.isEmpty() || rxe.isEmpty()) return showError(error, details);
    auto &brick = nxt::Brick::instance();
    if (!brick.connected()) return showError(tr("The NXT was disconnected. Connect it again and retry."));
    const QString file = m_name->text() + QStringLiteral(".rxe");
    setWorking(true, tr("Sending %1 to the brick…").arg(file));
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progressConn = connect(&brick, &nxt::Brick::jobProgress, m_progress, &QProgressBar::setValue);
    m_doneConn = connect(&brick, &nxt::Brick::jobFinished, this, &UploadDialog::finished);
    if (!brick.uploadProgram(file, rxe, m_run->isChecked())) {
        disconnect(m_progressConn);
        disconnect(m_doneConn);
        showError(tr("The brick is busy with another task. Wait a moment and try again."));
    }
}

void UploadDialog::finished(const QString &error)
{
    disconnect(m_progressConn);
    disconnect(m_doneConn);
    if (!error.isEmpty()) return showError(tr("Upload failed: %1").arg(error));
    const QString file = m_name->text() + QStringLiteral(".rxe");
    emit message(m_run->isChecked() ? tr("“%1” is on the brick and running").arg(file) : tr("“%1” is on the brick").arg(file), "success");
    accept();
}
