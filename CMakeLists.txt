cmake_minimum_required(VERSION 3.21)
project(UrsfAutoClicker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(APP_SOURCES
    src/main.cpp

    # core (no Qt Widgets)
    src/core/app_settings.h
    src/core/clicker_engine.cpp
    src/core/clicker_engine.h
    src/core/clicker_controller.cpp
    src/core/clicker_controller.h
    src/core/config_store.cpp
    src/core/config_store.h

    # platform (common)
    src/platform/app_filter.cpp
    src/platform/app_filter.h
    src/platform/hotkey_backend.h
    src/platform/key_mapping.cpp
    src/platform/key_mapping.h

    # ui
    src/ui/choose_apps_dialog.cpp
    src/ui/choose_apps_dialog.h
    src/ui/dock.h
    src/ui/icon_theme.cpp
    src/ui/icon_theme.h
    src/ui/main_window.cpp
    src/ui/main_window.h
    src/ui/tray_controller.cpp
    src/ui/tray_controller.h
    src/ui/pages/clicker_page.cpp
    src/ui/pages/clicker_page.h
    src/ui/pages/settings_page.cpp
    src/ui/pages/settings_page.h

    resources.qrc
)

add_executable(autoclicker ${APP_SOURCES})

# "core/...", "platform/...", "ui/..." style includes
target_include_directories(autoclicker PRIVATE src)

target_link_libraries(autoclicker Qt6::Core Qt6::Gui Qt6::Widgets)

# std::thread needs pthread outside of Windows/MSVC
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(autoclicker Threads::Threads)
endif()

# One hotkey backend per platform (the only place the OS is chosen).
if(WIN32)
    target_sources(autoclicker PRIVATE src/platform/hotkey_win.cpp)
    # GetAsyncKeyState (hotkey poll) + mouse_event (click simulation)
    target_link_libraries(autoclicker user32)

elseif(APPLE)
    target_sources(autoclicker PRIVATE src/platform/hotkey_null.cpp)
    target_link_libraries(autoclicker "-framework Cocoa" "-framework CoreGraphics")

else()
    # Linux: X11 is required (XGrabKey); XTest is optional and
    # clicker_engine falls back to xdotool under pure Wayland sessions.
    find_package(X11 REQUIRED)
    target_link_libraries(autoclicker ${X11_LIBRARIES})
    target_include_directories(autoclicker PRIVATE ${X11_INCLUDE_DIR})
    if(X11_Xtst_LIB)
        target_link_libraries(autoclicker ${X11_Xtst_LIB})
    endif()

    target_sources(autoclicker PRIVATE
        src/platform/hotkey_x11.cpp
        src/platform/x11_hotkey.cpp)
endif()

install(TARGETS autoclicker DESTINATION bin)
