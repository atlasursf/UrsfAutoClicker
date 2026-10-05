/**
 * App filter - lists open application windows and identifies the one that
 * currently has focus, so the global hotkey can be restricted to only fire
 * while a chosen app (a game, say) is frontmost.
 *
 * Kept free of X11/WinAPI includes in the header for the same reason as
 * x11_hotkey.h: this header is included from frontend.cpp, a Q_OBJECT
 * translation unit parsed by moc. All platform-specific code lives in
 * app_filter.cpp instead.
 */
#pragma once

#include <string>
#include <vector>

// A single running application/window, as shown in the "Choose Applications"
// dialog. `id` is what gets stored in config and matched against the
// currently focused app (WM_CLASS instance name on Linux, exe basename on
// Windows); `display_name` is just for the dialog list.
struct AppInfo {
    std::string id;
    std::string display_name;
};

// Enumerates currently open, visible top-level application windows, one
// entry per application (not per window). Returns an empty list where this
// isn't supported in this build (macOS) or the information isn't available
// (no X11/EWMH window manager) -- the dialog shows "No windows detected"
// and filtering still behaves as "allow all" in that case.
std::vector<AppInfo> list_running_apps();

// Identifies the application that currently has focus, in the same id
// space as list_running_apps(). Empty string if unknown/unsupported.
std::string active_app_identifier();
