/**
 * ClickerEngine - platform mouse simulation + click-timing engine.
 *
 * Runs the click loop on its own std::thread, guarded by a mutex so
 * settings can be swapped in live via update_config() while clicking
 * (mirrors the old backend's behavior, just without the TCP hop).
 *
 * Kept free of Xlib/CoreGraphics/WinAPI includes in the header, for the
 * same reason as x11_hotkey.h: this header is included from frontend.cpp,
 * which is a Q_OBJECT translation unit parsed by moc, and X11's macros
 * (Bool, True, False, None, Status, ...) collide with Qt's identifiers.
 * All platform-specific includes live in clicker_engine.cpp instead.
 */
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

struct ClickerConfig {
    double cps = 20.0;
    int duty_cycle = 25;   // percent
    bool randomize = false;
    bool click_limit_enabled = false;
    int click_limit = 100;
    int mouse_button = 0;  // 0=left, 1=right, 2=middle
};

class ClickerEngine {
public:
    ClickerEngine();
    ~ClickerEngine();

    void start(const ClickerConfig& cfg);
    void stop();

    // Applies new settings to an already-running clicker; picked up on the
    // next click. No restart, no counter reset. Safe to call whether or
    // not the engine is currently enabled.
    void update_config(const ClickerConfig& cfg);

    bool is_enabled() const;
    int get_click_count() const;
    std::string get_status() const;

private:
    void click_loop();

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_;
    int click_count_;
    ClickerConfig config_;
    std::thread* click_thread_;
};
