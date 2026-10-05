#pragma once

/**
 * ClickerPage - Activation / Clicks / Click Rate / Click Limit groups and
 * the status line. It reads and writes AppSettings and knows nothing about
 * the engine, hotkey backend or config file.
 */

#include <QStringList>
#include <QWidget>

#include "core/app_settings.h"
#include "core/clicker_controller.h"

class QCheckBox;
class QDoubleSpinBox;
class QKeyEvent;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;

class ClickerPage : public QWidget {
    Q_OBJECT

public:
    explicit ClickerPage(QWidget* parent = nullptr);

    void loadFrom(const AppSettings& settings);   // does not emit changed()
    void applyTo(AppSettings& settings) const;    // writes only the fields this page owns

    void setState(ClickerController::State state);
    void showHotkeyError(const QString& message);

signals:
    void changed();
    void keySelectionStarted();    // window should release the global hotkey
    void keySelectionFinished();   // ...and re-arm it with the new key

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void buildUi();
    void startKeySelection();
    void chooseApps();
    void notifyChanged();
    void updateAppStatusLabel();
    void updateStatus();

    QLabel* activation_label_;
    QPushButton* select_button_;
    QRadioButton* hold_radio_;
    QRadioButton* toggle_radio_;
    QPushButton* choose_apps_button_;
    QLabel* app_status_label_;

    QRadioButton* left_radio_;
    QRadioButton* right_radio_;
    QRadioButton* middle_radio_;

    QDoubleSpinBox* cps_spin_;
    QCheckBox* randomize_check_;
    QSpinBox* duty_spin_;

    QCheckBox* limit_check_;
    QSpinBox* limit_spin_;

    QLabel* status_label_;

    QString activation_key_ = "F6";
    QStringList selected_apps_;
    ClickerController::State state_ = ClickerController::State::Idle;
    bool selecting_key_ = false;
    bool loading_ = false;   // suppress changed() while loadFrom() fills widgets
};
