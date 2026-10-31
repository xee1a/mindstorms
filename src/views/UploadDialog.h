#pragma once

#include <QDialog>
#include <QJsonObject>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

/**
 * Names the program, compiles the blocks to an .rxe and downloads it to the brick,
 * optionally starting it. Stays open with the reason when something goes wrong.
 */
class UploadDialog : public QDialog {
    Q_OBJECT
public:
    UploadDialog(const QJsonObject &workspace, const QString &projectName, QWidget *parent = nullptr);

    /** A valid NXT file name (without .rxe) suggested from the project name. */
    static QString suggestedName(const QString &projectName);

    void reject() override;

signals:
    void message(const QString &text, const char *kind);

private:
    void start();
    void compiled(const QByteArray &rxe, const QString &error, const QString &details);
    void finished(const QString &error);
    void setWorking(bool on, const QString &status = {});
    void showError(const QString &text, const QString &details = {});

    QJsonObject m_workspace;
    QString m_projectName;
    QLineEdit *m_name;
    QCheckBox *m_run;
    QLabel *m_status;
    QLabel *m_error;
    QProgressBar *m_progress;
    QPushButton *m_upload;
    QPushButton *m_cancel;
    QPushButton *m_details;
    QString m_detailsText;
    QMetaObject::Connection m_progressConn;
    QMetaObject::Connection m_doneConn;
    bool m_working = false;
};
