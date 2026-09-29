/**
 * ClickerEngine implementation - platform mouse simulation lives here.
 *
 * This file has no Q_OBJECT of its own, so AUTOMOC never runs moc over it
 * and these platform headers never get anywhere near Qt's headers (same
 * split as x11_hotkey.h / x11_hotkey.cpp).
 *
 * Platform detection:
 *   - X11: XTest (fast native)
 *   - Wayland: xdotool (slower but works)
 *   - Windows: WinAPI
 *   - macOS: CoreGraphics
 */

#include "clicker_engine.h"

#include <algorithm>
#include <cstdlib>
#include <random>

// =========================================================================
// Platform Detection and Mouse Control
// =========================================================================

namespace {

bool is_wayland() {
    const char* xdg_session_type = std::getenv("XDG_SESSION_TYPE");
    return xdg_session_type && std::string(xdg_session_type) == "wayland";
}

bool is_x11() {
    const char* display = std::getenv("DISPLAY");
    return display != nullptr;
}

}  // namespace

#ifdef _WIN32
// Windows native mouse
#include <windows.h>

namespace {

void mouse_press(int button) {
    uint32_t flag_down = 0;
    if (button == 0) flag_down = MOUSEEVENTF_LEFTDOWN;
    else if (button == 1) flag_down = MOUSEEVENTF_RIGHTDOWN;
    else if (button == 2) flag_down = MOUSEEVENTF_MIDDLEDOWN;
    mouse_event(flag_down, 0, 0, 0, 0);
}

void mouse_release(int button) {
    uint32_t flag_up = 0;
    if (button == 0) flag_up = MOUSEEVENTF_LEFTUP;
    else if (button == 1) flag_up = MOUSEEVENTF_RIGHTUP;
    else if (button == 2) flag_up = MOUSEEVENTF_MIDDLEUP;
    mouse_event(flag_up, 0, 0, 0, 0);
}

}  // namespace

#elif defined(__APPLE__)
// macOS native mouse
#include <CoreGraphics/CoreGraphics.h>

namespace {

void mouse_press(int button) {
    CGEventType down_type;
    CGMouseButton cg_button;

    if (button == 0) {
        down_type = kCGEventLeftMouseDown;
        cg_button = kCGMouseButtonLeft;
    } else if (button == 1) {
        down_type = kCGEventRightMouseDown;
        cg_button = kCGMouseButtonRight;
    } else {
        down_type = kCGEventOtherMouseDown;
        cg_button = kCGMouseButtonCenter;
    }

    CGPoint pos = CGEventGetLocation(CGEventCreate(NULL));
    CGEventRef event = CGEventCreateMouseEvent(NULL, down_type, pos, cg_button);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

void mouse_release(int button) {
    CGEventType up_type;
    CGMouseButton cg_button;

    if (button == 0) {
        up_type = kCGEventLeftMouseUp;
        cg_button = kCGMouseButtonLeft;
    } else if (button == 1) {
        up_type = kCGEventRightMouseUp;
        cg_button = kCGMouseButtonRight;
    } else {
        up_type = kCGEventOtherMouseUp;
        cg_button = kCGMouseButtonCenter;
    }

    CGPoint pos = CGEventGetLocation(CGEventCreate(NULL));
    CGEventRef event = CGEventCreateMouseEvent(NULL, up_type, pos, cg_button);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

}  // namespace

#else
// Linux: X11 or Wayland
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

namespace {

Display* x11_display = nullptr;
bool x11_available = false;

void init_x11() {
    if (!x11_available && !x11_display) {
        x11_display = XOpenDisplay(nullptr);
        x11_available = (x11_display != nullptr);
    }
}

void mouse_press_x11(int button) {
    if (!x11_display) init_x11();
    if (x11_display) {
        unsigned int btn = (button == 1) ? Button3 : (button == 2) ? Button2 : Button1;
        XTestFakeButtonEvent(x11_display, btn, True, CurrentTime);
        XFlush(x11_display);
    }
}

void mouse_release_x11(int button) {
    if (!x11_display) init_x11();
    if (x11_display) {
        unsigned int btn = (button == 1) ? Button3 : (button == 2) ? Button2 : Button1;
        XTestFakeButtonEvent(x11_display, btn, False, CurrentTime);
        XFlush(x11_display);
    }
}

// Smart wrapper
void mouse_press(int button) {
    if (is_x11()) {
        mouse_press_x11(button);
    } else if (is_wayland()) {
        int btn_str = (button == 0) ? 1 : (button == 1) ? 3 : 2;
        std::string cmd = "xdotool mousedown " + std::to_string(btn_str) + " 2>/dev/null";
        system(cmd.c_str());
    }
}

void mouse_release(int button) {
    if (is_x11()) {
        mouse_release_x11(button);
    } else if (is_wayland()) {
        int btn_str = (button == 0) ? 1 : (button == 1) ? 3 : 2;
        std::string cmd = "xdotool mouseup " + std::to_string(btn_str) + " 2>/dev/null";
        system(cmd.c_str());
    }
}

}  // namespace

#endif

// =========================================================================
// ClickerEngine
// =========================================================================

ClickerEngine::ClickerEngine()
    : enabled_(false), click_count_(0), click_thread_(nullptr) {}

ClickerEngine::~ClickerEngine() {
    stop();
}

void ClickerEngine::start(const ClickerConfig& cfg) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (enabled_) return;
        config_ = cfg;
        click_count_ = 0;
        enabled_ = true;
    }

    if (click_thread_) {
        click_thread_->join();
        delete click_thread_;
    }

    click_thread_ = new std::thread([this]() { click_loop(); });
}

void ClickerEngine::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }
    if (click_thread_) {
        click_thread_->join();
        delete click_thread_;
        click_thread_ = nullptr;
    }
}

void ClickerEngine::update_config(const ClickerConfig& cfg) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = cfg;
}

bool ClickerEngine::is_enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

int ClickerEngine::get_click_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return click_count_;
}

std::string ClickerEngine::get_status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) return "stopped";
    return std::string("clicking:") + std::to_string(click_count_);
}

void ClickerEngine::click_loop() {
    std::mt19937 rng(std::random_device{}());

    while (enabled_) {
        ClickerConfig cfg;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!enabled_) break;
            cfg = config_;
        }

        double cps = std::max(1.0, cfg.cps);
        double interval = 1000000.0 / cps;  // microseconds

        if (cfg.randomize) {
            std::uniform_real_distribution<double> dist(-0.15, 0.15);
            double variation = dist(rng);
            interval *= (1.0 + variation);
        }

        double duty = cfg.duty_cycle / 100.0;
        double down_time_us = interval * duty;
        double up_time_us = interval * (1.0 - duty);

        down_time_us = std::max(100.0, down_time_us);
        up_time_us = std::max(100.0, up_time_us);

        mouse_press(cfg.mouse_button);
        std::this_thread::sleep_for(std::chrono::microseconds((long)down_time_us));
        mouse_release(cfg.mouse_button);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            click_count_++;

            if (cfg.click_limit_enabled && click_count_ >= cfg.click_limit) {
                enabled_ = false;
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::microseconds((long)up_time_us));
    }
}
