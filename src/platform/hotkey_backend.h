#pragma once

/**
 * HotkeyBackend - platform-neutral global hotkey interface.
 *
 * The window/controller code never sees an #ifdef: it asks
 * createHotkeyBackend() for the implementation of the current platform.
 * Exactly one of these is compiled in (selected in CMakeLists.txt):
 *   hotkey_win.cpp   Windows  (GetAsyncKeyState polling)
 *   hotkey_x11.cpp   Linux    (XGrabKey via X11HotkeyListener)
 *   hotkey_null.cpp  others   (no-op)
 *
 * Signals are always emitted on the thread the backend lives in (the
 * GUI thread), regardless of which thread detects the key.
 */

#include <QObject>
#include <QString>

class HotkeyBackend : public QObject {
    Q_OBJECT

public:
    explicit HotkeyBackend(QObject* parent = nullptr) : QObject(parent) {}
    ~HotkeyBackend() override = default;

    // Takes a QKeySequence text ("F6", "Esc"...). Safe to call while running.
    virtual void setKey(const QString& qtKeyName) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void pressed();    // once per physical press (no auto-repeat)
    void released();   // needed by Hold mode
    void error(const QString& message);
};

// Defined in exactly one platform .cpp. Result is owned by `parent`.
HotkeyBackend* createHotkeyBackend(QObject* parent);
