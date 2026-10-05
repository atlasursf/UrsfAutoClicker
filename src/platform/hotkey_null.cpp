/**
 * Fallback hotkey backend (macOS / unsupported platforms): does nothing.
 * The tray menu's "Toggle Clicker" still works.
 */

#include "platform/hotkey_backend.h"

namespace {

class NullHotkeyBackend : public HotkeyBackend {
public:
    explicit NullHotkeyBackend(QObject* parent) : HotkeyBackend(parent) {}
    void setKey(const QString&) override {}
    void start() override {}
    void stop() override {}
};

}  // namespace

HotkeyBackend* createHotkeyBackend(QObject* parent) {
    return new NullHotkeyBackend(parent);
}
