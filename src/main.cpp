#include "app/MainWindow.h"
#include "app/Splash.h"
#include "app/Theme.h"
#include "nxt/Brick.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("NXT Studio"));
    QApplication::setApplicationName(QStringLiteral("NXT Studio"));
    QApplication::setApplicationVersion(QStringLiteral(NXTSTUDIO_VERSION));

    // Polish by default; English is the source language.
    const QString lang = QSettings().value(QStringLiteral("language"), QStringLiteral("pl")).toString();
    QTranslator appTr;
    QTranslator qtTr;
    if (lang != QLatin1String("en")) {
        if (appTr.load(QStringLiteral(":/i18n/nxtstudio_%1.qm").arg(lang))) QApplication::installTranslator(&appTr);
        // Qt's own strings (dialog buttons and so on), deployed next to the exe by windeployqt.
        if (qtTr.load(QStringLiteral("qt_%1").arg(lang), QApplication::applicationDirPath() + QStringLiteral("/translations")) ||
            qtTr.load(QStringLiteral("qt_%1").arg(lang), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            QApplication::installTranslator(&qtTr);
    }

    theme::init();
    theme::apply(app);
    auto *splash = new Splash;
    splash->show();
    splash->setStatus(QCoreApplication::translate("Splash", "Loading blocks…"));
    MainWindow w;
    theme::styleTitleBar(&w);
    splash->setStatus(QCoreApplication::translate("Splash", "Looking for the NXT and the gamepad…"));
    splash->finishWhenReady(&w);
    const int rc = app.exec();
    nxt::Brick::instance().shutdown();
    return rc;
}
