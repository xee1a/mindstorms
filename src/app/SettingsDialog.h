#pragma once

#include "drive/Mixer.h"

#include <QDialog>

class QLabel;
class QListWidget;
class QStackedWidget;

/** Settings window: appearance, editor, drive setup, brick and about. */
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

signals:
    /** The user asked to restart now so theme, accent or language changes apply. */
    void restartRequested();
    void message(const QString &text, const char *kind);

private:
    QWidget *buildAppearance();
    QWidget *buildEditor();
    QWidget *buildDrive();
    QWidget *buildBrick();
    QWidget *buildAbout();
    void addPage(const char *icon, const QString &title, QWidget *page);
    /** Shows the restart banner when a restart-only setting differs from what the app started with. */
    void updateRestartBanner();
    void saveDrive();

    QListWidget *m_nav;
    QStackedWidget *m_pages;
    QWidget *m_restartBanner = nullptr;
    QString m_startTheme;
    QString m_startAccent;
    QString m_startLanguage;
    drive::DriveConfig m_drive;
    QLabel *m_modeHint = nullptr;
};
