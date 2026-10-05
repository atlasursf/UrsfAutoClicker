#include "ui/main_window.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStackedWidget>

#include "core/config_store.h"
#include "platform/hotkey_backend.h"
#include "ui/dock.h"
#include "ui/pages/clicker_page.h"
#include "ui/pages/settings_page.h"
#include "ui/tray_controller.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("URSF AutoClicker");
    resize(640, 550);

    controller_ = new ClickerController(this);
    hotkey_ = createHotkeyBackend(this);
    tray_ = new TrayController(this);

    buildUi();
    wireSignals();

    loadSettings(ConfigStore::load());
    tray_->show();
    hotkey_->start();

    // Settings are saved when the app exits however that happens
    // (window close or tray "Quit", which never triggers closeEvent).
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSettings);
}

void MainWindow::buildUi() {
    auto* root = new QWidget(this);
    setCentralWidget(root);

    auto* root_layout = new QHBoxLayout(root);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    dock_ = new Dock(this);
    stack_ = new QStackedWidget(this);
    root_layout->addWidget(dock_);
    root_layout->addWidget(stack_, 1);

    clicker_page_ = new ClickerPage(this);
    settings_page_ = new SettingsPage(this);
    stack_->addWidget(clicker_page_);    // index 0
    stack_->addWidget(settings_page_);   // index 1

    dock_->addButton("🖱", "Clicker", "Auto Clicker", [this]() { stack_->setCurrentIndex(0); });
    dock_->addButton("⚙", "Settings", "Settings", [this]() { stack_->setCurrentIndex(1); });
    dock_->addStretch();
    dock_->addButton("⏻", "Exit", "Exit", [this]() {
        const auto result = QMessageBox::question(
            this, "Exit", "Are you sure you want to exit the application?");
        if (result == QMessageBox::Yes) qApp->quit();
    }, false);  // sticky=false: Exit never stays highlighted
    dock_->setActive(0);
}

void MainWindow::wireSignals() {
    // controller -> UI
    connect(controller_, &ClickerController::stateChanged, this, &MainWindow::onStateChanged);

    // pages -> settings -> controller / icons
    connect(clicker_page_, &ClickerPage::changed, this, &MainWindow::onClickerPageChanged);
    connect(settings_page_, &SettingsPage::changed, this, &MainWindow::onSettingsPageChanged);

    // key selection: release the global grab while capturing, re-arm after
    connect(clicker_page_, &ClickerPage::keySelectionStarted, hotkey_, &HotkeyBackend::stop);
    connect(clicker_page_, &ClickerPage::keySelectionFinished, this,
            &MainWindow::onKeySelectionFinished);

    // hotkey -> controller
    connect(hotkey_, &HotkeyBackend::pressed, controller_, &ClickerController::onHotkeyPressed);
    connect(hotkey_, &HotkeyBackend::released, controller_, &ClickerController::onHotkeyReleased);
    connect(hotkey_, &HotkeyBackend::error, clicker_page_, &ClickerPage::showHotkeyError);

    // tray
    connect(tray_, &TrayController::toggleRequested, controller_, &ClickerController::toggle);
    connect(tray_, &TrayController::quitRequested, qApp, &QApplication::quit);
    connect(tray_, &TrayController::showHideRequested, this, &MainWindow::toggleVisibility);
}

void MainWindow::loadSettings(AppSettings s) {
    s.iconStyle = IconTheme::normalize(s.iconStyle);
    settings_ = s;

    clicker_page_->loadFrom(settings_);
    settings_page_->loadFrom(settings_);
    controller_->applySettings(settings_);
    hotkey_->setKey(settings_.activationKey);
    refreshIcons();
}

void MainWindow::saveSettings() {
    clicker_page_->applyTo(settings_);
    settings_page_->applyTo(settings_);
    ConfigStore::save(settings_);
}

void MainWindow::refreshIcons() {
    const QIcon icon = icons_.icon(settings_.iconStyle, controller_->isClicking());
    tray_->setIcon(icon);
    setWindowIcon(icon);
}

void MainWindow::onClickerPageChanged() {
    clicker_page_->applyTo(settings_);
    controller_->applySettings(settings_);
}

void MainWindow::onSettingsPageChanged() {
    settings_page_->applyTo(settings_);
    refreshIcons();
}

void MainWindow::onKeySelectionFinished() {
    clicker_page_->applyTo(settings_);
    hotkey_->setKey(settings_.activationKey);
    hotkey_->start();
}

void MainWindow::onStateChanged(ClickerController::State state) {
    clicker_page_->setState(state);
    refreshIcons();
}

void MainWindow::toggleVisibility() {
    if (isVisible()) {
        hide();
    } else {
        showNormal();
        activateWindow();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    hotkey_->stop();
    controller_->stop();
    QMainWindow::closeEvent(event);
}
