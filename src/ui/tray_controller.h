#pragma once

/**
 * TrayController - system tray icon + its context menu.
 * It only reports what the user asked for; the window decides what to do.
 */

#include <QIcon>
#include <QObject>
#include <QSystemTrayIcon>

class QMenu;
class QWidget;

class TrayController : public QObject {
    Q_OBJECT

public:
    explicit TrayController(QWidget* parentWindow);

    void setIcon(const QIcon& icon);
    void show();

signals:
    void toggleRequested();
    void quitRequested();
    void showHideRequested();   // double click on the icon

private:
    QSystemTrayIcon* tray_;
    QMenu* menu_;
};
