// prompter — drives the Elgato Prompter's screen directly and scrolls a script on it.

#include <QGuiApplication>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "drmoutput.h"
#include "preview.h"
#include "theme.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("prompter");
    app.setApplicationVersion("0.1.0");

    // Matches the window to prompter.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("prompter");
    app.setWindowIcon(QIcon::fromTheme("prompter"));

    QQuickStyle::setStyle("Material");
    qmlRegisterType<Preview>("Prompter", 1, 0, "Preview");

    Theme theme;
    DrmOutput output;
    Backend backend(&output);
    backend.setAccent(QColor(theme.accent()));
    QObject::connect(&theme, &Theme::changed, &backend, [&] { backend.setAccent(QColor(theme.accent())); });

    const QStringList arguments = app.arguments();
    if (arguments.size() > 1)
        backend.openFile(QUrl::fromUserInput(arguments.at(1), QDir::currentPath(), QUrl::AssumeLocalFile));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
