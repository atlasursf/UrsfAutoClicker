/**
 * Linux hotkey backend: thin adapter over X11HotkeyListener (XGrabKey).
 * x11_hotkey.h is X11-free, so nothing in this file touches Xlib macros.
 * No Q_OBJECT: signals are forwarded with signal->signal connections.
 */

#include "platform/hotkey_backend.h"
#include "platform/key_mapping.h"
#include "platform/x11_hotkey.h"

namespace {

class X11HotkeyBackend : public HotkeyBackend {
public:
    explicit X11HotkeyBackend(QObject* parent)
        : HotkeyBackend(parent), listener_(new X11HotkeyListener(this)) {
        // The listener emits from its own thread; these are queued
        // connections, so our signals fire on the GUI thread.
        connect(listener_, &X11HotkeyListener::hotkeyPressed, this, &HotkeyBackend::pressed);
        connect(listener_, &X11HotkeyListener::hotkeyReleased, this, &HotkeyBackend::released);
        connect(listener_, &X11HotkeyListener::errorOccurred, this, &HotkeyBackend::error);
    }

    ~X11HotkeyBackend() override { listener_->stop(); }

    void setKey(const QString& qtKeyName) override {
        const bool wasRunning = listener_->isRunning();
        if (wasRunning) listener_->stop();
        listener_->set_key_name(x11KeyName(qtKeyName));
        if (wasRunning) listener_->start();
    }

    void start() override {
        if (!listener_->isRunning()) listener_->start();
    }

    void stop() override { listener_->stop(); }

private:
    X11HotkeyListener* listener_;
};

}  // namespace

HotkeyBackend* createHotkeyBackend(QObject* parent) {
    return new X11HotkeyBackend(parent);
}
