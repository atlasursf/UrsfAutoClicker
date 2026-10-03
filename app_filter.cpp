/**
 * app_filter implementation - platform window/process enumeration lives
 * here. This file has no Q_OBJECT of its own, so AUTOMOC never runs moc
 * over it (same split as x11_hotkey.cpp / clicker_engine.cpp).
 */

#include "app_filter.h"

#ifdef __linux__
// =========================================================================
// Linux: X11 / EWMH (_NET_CLIENT_LIST, _NET_ACTIVE_WINDOW, WM_CLASS)
// =========================================================================
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

namespace {

Atom atom(Display* d, const char* name) {
    return XInternAtom(d, name, False);
}

// WM_CLASS's "class" string (e.g. "firefox", "Steam", "jetbrains-idea") is
// the closest thing X11 has to a stable per-application id, so it doubles
// as both the match key and (when no title is available) the display name.
std::string window_class_name(Display* d, Window w) {
    XClassHint hint{nullptr, nullptr};
    std::string result;
    if (XGetClassHint(d, w, &hint)) {
        if (hint.res_class) result = hint.res_class;
        else if (hint.res_name) result = hint.res_name;
        if (hint.res_name) XFree(hint.res_name);
        if (hint.res_class) XFree(hint.res_class);
    }
    return result;
}

std::string window_title(Display* d, Window w) {
    Atom net_wm_name = atom(d, "_NET_WM_NAME");
    Atom utf8_string = atom(d, "UTF8_STRING");
    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(d, w, net_wm_name, 0, 1024, False, utf8_string,
                           &actual_type, &actual_format, &n_items, &bytes_after, &data) == Success
        && data) {
        std::string title(reinterpret_cast<char*>(data), n_items);
        XFree(data);
        if (!title.empty()) return title;
    }

    char* name = nullptr;
    if (XFetchName(d, w, &name) && name) {
        std::string title(name);
        XFree(name);
        return title;
    }
    return "";
}

}  // namespace

std::vector<AppInfo> list_running_apps() {
    std::vector<AppInfo> result;

    Display* d = XOpenDisplay(nullptr);
    if (!d) return result;

    Window root = DefaultRootWindow(d);
    Atom client_list = atom(d, "_NET_CLIENT_LIST");

    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(d, root, client_list, 0, ~0L, False, XA_WINDOW,
                           &actual_type, &actual_format, &n_items, &bytes_after, &data) != Success
        || !data) {
        if (data) XFree(data);
        XCloseDisplay(d);
        return result;  // window manager doesn't publish _NET_CLIENT_LIST
    }

    Window* windows = reinterpret_cast<Window*>(data);
    for (unsigned long i = 0; i < n_items; ++i) {
        std::string cls = window_class_name(d, windows[i]);
        if (cls.empty()) continue;

        bool already_added = false;
        for (const AppInfo& app : result) {
            if (app.id == cls) { already_added = true; break; }
        }
        if (already_added) continue;  // one entry per application, not per window

        std::string title = window_title(d, windows[i]);
        result.push_back({cls, title.empty() ? cls : title});
    }

    XFree(data);
    XCloseDisplay(d);
    return result;
}

std::string active_app_identifier() {
    Display* d = XOpenDisplay(nullptr);
    if (!d) return "";

    Window root = DefaultRootWindow(d);
    Atom active_window = atom(d, "_NET_ACTIVE_WINDOW");

    Atom actual_type;
    int actual_format;
    unsigned long n_items, bytes_after;
    unsigned char* data = nullptr;

    std::string result;
    if (XGetWindowProperty(d, root, active_window, 0, 1, False, XA_WINDOW,
                           &actual_type, &actual_format, &n_items, &bytes_after, &data) == Success
        && data && n_items > 0) {
        Window active = *reinterpret_cast<Window*>(data);
        result = window_class_name(d, active);
    }
    if (data) XFree(data);
    XCloseDisplay(d);
    return result;
}

#elif defined(_WIN32)
// =========================================================================
// Windows: EnumWindows + GetForegroundWindow, matched by exe basename
// =========================================================================
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

// UTF-16 -> UTF-8. A plain per-character copy would mangle any non-ASCII
// window title (Turkish ş/ğ/ı, etc.); the Qt side reads these as UTF-8.
std::string to_utf8(const wchar_t* wstr, int len) {
    if (len <= 0) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, &out[0], size, nullptr, nullptr);
    return out;
}

// Resolves a process id to its executable's basename (e.g. "steam.exe").
// Requires only PROCESS_QUERY_LIMITED_INFORMATION, so this works without
// elevation even for processes we don't own.
std::string exe_basename_from_pid(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return "";

    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    std::string result;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        std::wstring wpath(path, size);
        size_t slash = wpath.find_last_of(L"\\/");
        std::wstring base = (slash == std::wstring::npos) ? wpath : wpath.substr(slash + 1);
        result = to_utf8(base.c_str(), static_cast<int>(base.size()));
    }
    CloseHandle(process);
    return result;
}

BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lparam) {
    auto* out = reinterpret_cast<std::vector<AppInfo>*>(lparam);

    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t title[256];
    int len = GetWindowTextW(hwnd, title, 256);
    if (len == 0) return TRUE;  // untitled windows are rarely real apps

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    std::string id = exe_basename_from_pid(pid);
    if (id.empty()) return TRUE;

    for (const AppInfo& app : *out) {
        if (app.id == id) return TRUE;  // one entry per application, not per window
    }

    out->push_back({id, to_utf8(title, len)});
    return TRUE;
}

}  // namespace

std::vector<AppInfo> list_running_apps() {
    std::vector<AppInfo> result;
    EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&result));
    return result;
}

std::string active_app_identifier() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return "";
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return exe_basename_from_pid(pid);
}

#else
// =========================================================================
// macOS: not implemented in this build (the global hotkey itself is
// already a no-op on macOS - see check_hotkey() in frontend.cpp). An empty
// list / empty id means is_app_allowed() always allows, so the filter has
// no effect here rather than silently blocking everything.
// =========================================================================

std::vector<AppInfo> list_running_apps() {
    return {};
}

std::string active_app_identifier() {
    return "";
}

#endif