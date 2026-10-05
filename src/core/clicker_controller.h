#pragma once

/**
 * ClickerController - owns the ClickerEngine and the start/stop state machine.
 *
 * Responsibilities moved out of the window class:
 *   - Hold / Toggle logic
 *   - application filter gate (is the focused app allowed?)
 *   - pushing live settings into a running engine
 *   - noticing a click-limit stop on the engine thread (polling)
 *
 * No Qt Widgets here; the UI just reacts to stateChanged().
 */

#include <QObject>
#include <QTimer>

#include "core/app_settings.h"
#include "core/clicker_engine.h"

class ClickerController : public QObject {
    Q_OBJECT

public:
    enum class State { Idle, Clicking, StoppedByLimit };

    explicit ClickerController(QObject* parent = nullptr);
    ~ClickerController() override;

    State state() const { return state_; }
    bool isClicking() const { return state_ == State::Clicking; }

public slots:
    void applySettings(const AppSettings& settings);

    void start();
    void stop();
    void toggle();                 // tray menu etc. (not filtered by app list)

    void onHotkeyPressed();        // honours Hold/Toggle and the app filter
    void onHotkeyReleased();

signals:
    void stateChanged(ClickerController::State state);

private:
    void setState(State state);
    bool isAppAllowed() const;
    void syncWithEngine();

    ClickerEngine engine_;
    QTimer* syncTimer_;
    AppSettings settings_;
    State state_ = State::Idle;
};
