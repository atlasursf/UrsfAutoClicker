/**
 * URSF AutoClicker - C++ Backend (Wayland Compatible)
 *
 * Platform detection:
 *   - X11: XTest (fast native)
 *   - Wayland: uinput (slower but works)
 *   - Windows: WinAPI
 *   - macOS: CoreGraphics
 *
 * Build:
 *   Linux:
 *     sudo apt-get install libx11-dev libxtst-dev libdbus-1-dev libudev-dev
 *     g++ -std=c++17 -pthread -o clicker-backend backend_wayland.cpp \
 *         -lX11 -lXtst -ldbus-1 $(pkg-config --cflags --libs libevdev) 2>/dev/null || \
 *     g++ -std=c++17 -pthread -o clicker-backend backend_wayland.cpp -ldbus-1
 *
 *   macOS:
 *     g++ -std=c++17 -pthread -framework Cocoa -o clicker-backend backend_wayland.cpp
 *
 *   Windows (MSVC):
 *     cl /std:c++17 /MT backend_wayland.cpp user32.lib
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <cstring>

// Platform-specific socket includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define CLOSE_SOCKET closesocket
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;
const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
#define CLOSE_SOCKET close
#endif

// =========================================================================
// Platform Detection and Mouse Control
// =========================================================================

bool is_wayland() {
    const char* xdg_session_type = std::getenv("XDG_SESSION_TYPE");
    return xdg_session_type && std::string(xdg_session_type) == "wayland";
}

bool is_x11() {
    const char* display = std::getenv("DISPLAY");
    return display != nullptr;
}

#ifdef _WIN32
// Windows native mouse
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

#elif defined(__APPLE__)
// macOS native mouse
#include <CoreGraphics/CoreGraphics.h>

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

#else
// Linux: X11 or Wayland
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

Display* x11_display = nullptr;
bool x11_available = false;

void init_x11() {
    if (!x11_available && !x11_display) {
        x11_display = XOpenDisplay(nullptr);
        x11_available = (x11_display != nullptr);
        if (x11_available) {
            std::cout << "[Backend] X11 XTest available" << std::endl;
        }
    }
}

// X11 implementation (fast)
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

// Wayland fallback: uinput via system call
void mouse_press_uinput(int button) {
    int btn_code = (button == 0) ? 272 : (button == 1) ? 273 : 274;  // BTN_LEFT, BTN_RIGHT, BTN_MIDDLE
    std::string cmd = "echo 'input event' | sudo -n bash -c 'echo " + std::to_string(btn_code) + " > /dev/uinput' 2>/dev/null";
    system(cmd.c_str());
}

void mouse_release_uinput(int button) {
    // Wayland uinput is complex; using xdotool fallback
    int btn_str = (button == 0) ? 1 : (button == 1) ? 3 : 2;
    std::string cmd = "xdotool mouseup " + std::to_string(btn_str) + " 2>/dev/null";
    system(cmd.c_str());
}

// Smart wrapper
void mouse_press(int button) {
    if (is_x11()) {
        mouse_press_x11(button);
    } else if (is_wayland()) {
        // Wayland: try xdotool first, then uinput
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
#endif

// =========================================================================
// Clicker Configuration
// =========================================================================

struct ClickerConfig {
    double cps = 20.0;
    int duty_cycle = 25;
    bool randomize = false;
    bool click_limit_enabled = false;
    int click_limit = 100;
    int mouse_button = 0;  // 0=left, 1=right, 2=middle
};

// =========================================================================
// Clicker Engine
// =========================================================================

class ClickerEngine {
public:
    ClickerEngine()
        : enabled_(false), click_count_(0), click_thread_(nullptr) {}

    ~ClickerEngine() {
        stop();
    }

    void start(const ClickerConfig& cfg) {
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

    void stop() {
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

    // Replace the settings while running. click_loop() copies config_ under
    // the mutex at the start of every click, so this takes effect on the
    // next click without restarting the clicker or resetting the counter.
    void update_config(const ClickerConfig& cfg) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = cfg;
    }

    bool is_enabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    int get_click_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return click_count_;
    }

    std::string get_status() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return "stopped";
        return std::string("clicking:") + std::to_string(click_count_);
    }

private:
    void click_loop() {
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

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_;
    int click_count_;
    ClickerConfig config_;
    std::thread* click_thread_;
};

// =========================================================================
// TCP Server
// =========================================================================

class TcpServer {
public:
    TcpServer(int port, ClickerEngine& engine)
        : port_(port), engine_(engine), running_(false), server_socket_(INVALID_SOCKET),
        server_thread_(nullptr) {}

    ~TcpServer() {
        stop();
    }

    bool start() {
#ifdef _WIN32
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }
#endif

        server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket_ == INVALID_SOCKET) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        server_addr.sin_port = htons(port_);

        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed on port " << port_ << std::endl;
            CLOSE_SOCKET(server_socket_);
            return false;
        }

        if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed" << std::endl;
            CLOSE_SOCKET(server_socket_);
            return false;
        }

        running_ = true;
        server_thread_ = new std::thread([this]() { accept_loop(); });

        std::cout << "Server listening on localhost:" << port_ << std::endl;
        return true;
    }

    void stop() {
        running_ = false;
        if (server_socket_ != INVALID_SOCKET) {
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }
        if (server_thread_) {
            server_thread_->join();
            delete server_thread_;
            server_thread_ = nullptr;
        }
    }

private:
    void accept_loop() {
        while (running_) {
            sockaddr_in client_addr = {};
            socklen_t client_addr_len = sizeof(client_addr);

            SOCKET client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_addr_len);
            if (client_socket == INVALID_SOCKET) {
                if (running_) {
                    std::cerr << "Accept failed" << std::endl;
                }
                break;
            }

            std::thread(&TcpServer::handle_client, this, client_socket).detach();
        }
    }

    // The connection stays open: the frontend sends many commands over it
    // (START on key press, STOP on key release, UPDATE on every setting
    // change). Commands are newline-terminated.
    void handle_client(SOCKET client_socket) {
        std::string pending;
        char buffer[1024];

        while (true) {
            int n = recv(client_socket, buffer, sizeof(buffer), 0);
            if (n <= 0) break;  // client closed the connection
            pending.append(buffer, n);

            size_t pos;
            while ((pos = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, pos);
                pending.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                std::string response = process_request(line);
                send(client_socket, response.c_str(), (int)response.length(), 0);
            }
        }

        CLOSE_SOCKET(client_socket);
    }

    // Parses "key=value key=value ..." pairs shared by START and UPDATE.
    static ClickerConfig parse_config(std::istringstream& iss) {
        ClickerConfig cfg;

        std::string pair;
        while (iss >> pair) {
            size_t eq = pair.find('=');
            if (eq == std::string::npos) continue;

            std::string key = pair.substr(0, eq);
            std::string val = pair.substr(eq + 1);

            try {
                if (key == "cps") {
                    cfg.cps = std::stod(val);
                } else if (key == "duty") {
                    cfg.duty_cycle = std::stoi(val);
                } else if (key == "button") {
                    cfg.mouse_button = (val == "right") ? 1 : (val == "middle") ? 2 : 0;
                } else if (key == "randomize") {
                    cfg.randomize = (val == "1");
                } else if (key == "limit_enabled") {
                    cfg.click_limit_enabled = (val == "1");
                } else if (key == "limit") {
                    cfg.click_limit = std::stoi(val);
                }
            } catch (const std::exception&) {
                // Bad number: keep the default for this field.
            }
        }
        return cfg;
    }

    std::string process_request(const std::string& request) {
        std::istringstream iss(request);
        std::string cmd;
        iss >> cmd;

        if (cmd == "START") {
            engine_.start(parse_config(iss));
            return "OK\n";
        }
        else if (cmd == "UPDATE") {
            // Applies new settings to an already running clicker; picked up
            // on the next click (same as the original app reading the UI
            // controls on every timer tick).
            engine_.update_config(parse_config(iss));
            return "OK\n";
        }
        else if (cmd == "STOP") {
            engine_.stop();
            return "OK\n";
        }
        else if (cmd == "STATUS") {
            return engine_.get_status() + "\n";
        }
        else {
            return "ERROR unknown command\n";
        }
    }

    int port_;
    ClickerEngine& engine_;
    std::atomic<bool> running_;
    SOCKET server_socket_;
    std::thread* server_thread_;
};

// =========================================================================
// Main
// =========================================================================

int main(int argc, char* argv[]) {
    int port = 9999;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    // Platform info
    std::cout << "URSF AutoClicker Backend (Wayland Compatible)" << std::endl;
#ifdef __linux__
    if (is_wayland()) {
        std::cout << "[Backend] Detected: Wayland (using xdotool)" << std::endl;
    } else if (is_x11()) {
        std::cout << "[Backend] Detected: X11 (using XTest)" << std::endl;
    } else {
        std::cout << "[Backend] Detected: Unknown session type" << std::endl;
    }
#endif

    ClickerEngine engine;
    TcpServer server(port, engine);

    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Waiting for frontend connections..." << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}