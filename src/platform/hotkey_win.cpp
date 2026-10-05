/**
 * Windows hotkey backend: polls GetAsyncKeyState every 10 ms.
 * No Q_OBJECT here, so AUTOMOC never parses windows.h-polluted code.
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "platform/hotkey_backend.h"
#include "platform/key_mapping.h"

#include <QTimer>

#include <windows.h>

namespace {

class WinHotkeyBackend : public HotkeyBackend {
public:
    explicit WinHotkeyBackend(QObject* parent) : HotkeyBackend(parent) {
        timer_ = new QTimer(this);
        timer_->setTimerType(Qt::PreciseTimer);
        connect(timer_, &QTimer::timeout, this, [this]() { poll(); });
    }

    void setKey(const QString& qtKeyName) override {
        vk_ = vkFromKeyName(qtKeyName);
        prevDown_ = false;
        if (vk_ == 0) {
            emit error("Unsupported key: " + qtKeyName);
        }
    }

    void start() override { timer_->start(10); }

    void stop() override {
        timer_->stop();
        prevDown_ = false;
    }

private:
    void poll() {
        if (vk_ == 0) return;  // unmapped key: nothing to poll

        const bool down = (GetAsyncKeyState(vk_) & 0x8000) != 0;
        if (down && !prevDown_) {
            emit pressed();
        } else if (!down && prevDown_) {
            emit released();
        }
        prevDown_ = down;
    }

    QTimer* timer_;
    int vk_ = 0;
    bool prevDown_ = false;
};

}  // namespace

HotkeyBackend* createHotkeyBackend(QObject* parent) {
    return new WinHotkeyBackend(parent);
}
