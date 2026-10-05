#include "core/clicker_controller.h"

#include <string>

#include "platform/app_filter.h"

ClickerController::ClickerController(QObject* parent) : QObject(parent) {
    // The engine can stop itself on its own thread (click limit) and
    // nothing pushes that to us, so poll.
    syncTimer_ = new QTimer(this);
    connect(syncTimer_, &QTimer::timeout, this, &ClickerController::syncWithEngine);
    syncTimer_->start(150);
}

ClickerController::~ClickerController() {
    engine_.stop();
}

void ClickerController::applySettings(const AppSettings& s) {
    const bool modeChanged = s.holdMode != settings_.holdMode;
    const bool keyChanged = s.activationKey != settings_.activationKey;
    settings_ = s;

    if (engine_.is_enabled()) {
        // Switching to Hold while clicking stops it (the key isn't held).
        if (modeChanged && settings_.holdMode) {
            stop();
        } else {
            engine_.update_config(toClickerConfig(settings_));
        }
    } else if (state_ == State::StoppedByLimit && (modeChanged || keyChanged)) {
        setState(State::Idle);  // drop the stale "limit reached" message
    }
}

void ClickerController::start() {
    engine_.start(toClickerConfig(settings_));
    setState(State::Clicking);
}

void ClickerController::stop() {
    engine_.stop();
    setState(State::Idle);
}

void ClickerController::toggle() {
    if (engine_.is_enabled()) {
        stop();
    } else {
        start();
    }
}

void ClickerController::onHotkeyPressed() {
    if (!isAppAllowed()) return;

    if (settings_.holdMode) {
        if (!engine_.is_enabled()) start();
    } else {
        toggle();
    }
}

void ClickerController::onHotkeyReleased() {
    if (settings_.holdMode && engine_.is_enabled()) stop();
}

// With no apps chosen everything is allowed. If the platform can't tell
// what's focused (macOS in this build) we don't block either: a filter
// with no effect beats one that silently disables the hotkey.
bool ClickerController::isAppAllowed() const {
    if (settings_.selectedApps.isEmpty()) return true;

    const std::string active = active_app_identifier();
    if (active.empty()) return true;

    return settings_.selectedApps.contains(QString::fromStdString(active));
}

void ClickerController::syncWithEngine() {
    if (state_ == State::Clicking && !engine_.is_enabled()) {
        setState(State::StoppedByLimit);
    }
}

void ClickerController::setState(State state) {
    if (state_ == state) return;
    state_ = state;
    emit stateChanged(state_);
}
