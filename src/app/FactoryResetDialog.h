#pragma once

#include "nxt/Brick.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;

/**
 * Lists the files on the brick, says exactly what a factory reset removes and keeps,
 * and deletes them after the user confirms. The firmware itself is never touched.
 */
class FactoryResetDialog : public QDialog {
    Q_OBJECT
public:
    explicit FactoryResetDialog(QWidget *parent = nullptr);
    void reject() override;

signals:
    void message(const QString &text, const char *kind);

private:
    void listed(const QList<nxt::BrickFile> &files);
    void updateSummary();
    void start();
    void finished(const QString &error);

    QList<nxt::BrickFile> m_files;
    QLabel *m_summary;
    QListWidget *m_list;
    QCheckBox *m_everything;
    QCheckBox *m_rename;
    QCheckBox *m_confirm;
    QProgressBar *m_progress;
    QPushButton *m_reset;
    QPushButton *m_cancel;
    QMetaObject::Connection m_progressConn;
    QMetaObject::Connection m_doneConn;
    bool m_loading = true;
    bool m_working = false;
};
