/**
 * URSF AutoClicker - Qt6 frontend entry point.
 *
 * Layout:
 *   core/      settings, config file, controller + click engine (no Widgets)
 *   platform/  hotkey backends, key mapping, app filter (OS-specific)
 *   ui/        MainWindow, dock, pages, tray, dialogs
 */

#include <QApplication>

#include "ui/main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("UrsfAutoClicker");  // config dir name

    MainWindow window;
    window.show();

    return app.exec();
}
