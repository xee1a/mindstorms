#include "app/MainWindow.h"
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

    theme::apply(app);
    MainWindow w;
    theme::darkTitleBar(&w);
    w.show();
    // `NXTStudio.exe --demo` starts connected to the simulated brick.
    if (QApplication::arguments().contains(QStringLiteral("--demo"))) nxt::Brick::instance().connectDemo();
    const int rc = app.exec();
    nxt::Brick::instance().shutdown();
    return rc;
}
