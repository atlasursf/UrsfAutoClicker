/**
 * X11 Global Hotkey Listener - Implementation
 *
 * This file is where all the X11/Xlib includes live. It is a plain .cpp
 * with no Q_OBJECT macro of its own (the class is declared in
 * x11_hotkey.h), so AUTOMOC never runs moc over this file and X11's
 * macros never get anywhere near Qt's headers.
 */

#include "x11_hotkey.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <unistd.h>
#include <string>

X11HotkeyListener::X11HotkeyListener(QObject* parent)
    : QThread(parent), running_(false), key_name_("F6") {}

X11HotkeyListener::~X11HotkeyListener() {
    stop();
}

void X11HotkeyListener::set_key_name(const QString& name) {
    key_name_ = name;
}

void X11HotkeyListener::stop() {
    if (isRunning()) {
        running_ = false;
        wait(1000);
    }
}

void X11HotkeyListener::run() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        emit errorOccurred("Cannot open X11 display (not running under X11/XWayland?)");
        return;
    }

    std::string key_std = key_name_.toStdString();
    KeySym keysym = XStringToKeysym(key_std.c_str());
    if (keysym == NoSymbol) {
        emit errorOccurred("Unknown key name: " + key_name_);
        XCloseDisplay(display);
        return;
    }

    KeyCode keycode = XKeysymToKeycode(display, keysym);
    if (keycode == 0) {
        emit errorOccurred("Cannot map key to a keycode: " + key_name_);
        XCloseDisplay(display);
        return;
    }

    Window root = DefaultRootWindow(display);

    // Grab with common lock-key modifier combinations too, so the
    // hotkey still fires when Caps Lock / Num Lock happen to be on.
    const unsigned int modifier_masks[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };

    // Ignore BadAccess if another app already grabbed this key.
    XErrorHandler old_handler = XSetErrorHandler([](Display*, XErrorEvent*) -> int { return 0; });

    for (unsigned int mod : modifier_masks) {
        XGrabKey(display, keycode, mod, root, True, GrabModeAsync, GrabModeAsync);
    }
    XSync(display, False);
    XSetErrorHandler(old_handler);

    // Without "detectable auto-repeat", holding a key makes X11 send a stream
    // of fake Release+Press pairs, which would make Hold mode flicker.
    XkbSetDetectableAutoRepeat(display, True, nullptr);

    XSelectInput(display, root, KeyPressMask | KeyReleaseMask);

    running_ = true;

    bool key_down = false;  // swallow auto-repeat presses while held

    XEvent event;
    while (running_) {
        if (XPending(display) > 0) {
            XNextEvent(display, &event);
            if (event.type == KeyPress || event.type == KeyRelease) {
                XKeyEvent* ke = reinterpret_cast<XKeyEvent*>(&event);
                if (ke->keycode == keycode) {
                    if (event.type == KeyPress && !key_down) {
                        key_down = true;
                        emit hotkeyPressed();
                    } else if (event.type == KeyRelease && key_down) {
                        key_down = false;
                        emit hotkeyReleased();
                    }
                }
            }
        } else {
            usleep(15000);  // 15ms poll so we can check running_ regularly
        }
    }

    for (unsigned int mod : modifier_masks) {
        XUngrabKey(display, keycode, mod, root);
    }
    XCloseDisplay(display);
}