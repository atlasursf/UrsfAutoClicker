#include "ui/tray_controller.h"

#include <QAction>
#include <QMenu>

TrayController::TrayController(QWidget* parentWindow) : QObject(parentWindow) {
    tray_ = new QSystemTrayIcon(this);

    menu_ = new QMenu(parentWindow);
    QAction* toggle_action = menu_->addAction("Toggle Clicker");
    connect(toggle_action, &QAction::triggered, this, &TrayController::toggleRequested);
    menu_->addSeparator();
    QAction* quit_action = menu_->addAction("Quit");
    connect(quit_action, &QAction::triggered, this, &TrayController::quitRequested);
    tray_->setContextMenu(menu_);

    connect(tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick) emit showHideRequested();
            });
}

void TrayController::setIcon(const QIcon& icon) {
    tray_->setIcon(icon);
}

void TrayController::show() {
    tray_->show();
}
