#pragma once

#include <QSplashScreen>

/** Startup screen with the logo, shown while the main window is built. Stays at least a moment so it doesn't flash. */
class Splash : public QSplashScreen {
    Q_OBJECT
public:
    Splash();
    void setStatus(const QString &text);
    /** Closes once the minimum time has passed, then shows `window`. */
    void finishWhenReady(QWidget *window);

protected:
    void drawContents(QPainter *p) override;

private:
    QString m_status;
    qint64 m_shownAt = 0;
};
