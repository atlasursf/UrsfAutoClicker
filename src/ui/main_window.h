#pragma once

/**
 * MainWindow - thin shell. It builds the dock + pages and wires the
 * controller, hotkey backend, tray and config store together.
 * It owns the master AppSettings copy.
 */

#include <QMainWindow>

#include "core/app_settings.h"
#include "core/clicker_controller.h"
#include "ui/icon_theme.h"

class ClickerPage;
class Dock;
class HotkeyBackend;
class QCloseEvent;
class QStackedWidget;
class SettingsPage;
class TrayController;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void wireSignals();

    void loadSettings(AppSettings settings);
    void saveSettings();

    void refreshIcons();
    void onClickerPageChanged();
    void onSettingsPageChanged();
    void onKeySelectionFinished();
    void onStateChanged(ClickerController::State state);
    void toggleVisibility();

    IconTheme icons_;
    AppSettings settings_;

    ClickerController* controller_;
    HotkeyBackend* hotkey_;
    TrayController* tray_;

    Dock* dock_;
    QStackedWidget* stack_;
    ClickerPage* clicker_page_;
    SettingsPage* settings_page_;
};
