cmake_minimum_required(VERSION 3.21)
project(UrsfAutoClicker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 (no Network component anymore: everything runs in one process)
find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    REQUIRED)

# Auto-generate MOC files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Single executable: GUI + click engine in one process
add_executable(autoclicker
    frontend.cpp
    clicker_engine.cpp
    clicker_engine.h
    app_filter.cpp
    app_filter.h
    resources.qrc
)

target_link_libraries(autoclicker
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# std::thread needs pthread outside of Windows/MSVC
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(autoclicker Threads::Threads)
endif()

if(WIN32)
    # GetAsyncKeyState (hotkey poll) + mouse_event (click simulation)
    target_link_libraries(autoclicker user32)

elseif(APPLE)
    target_link_libraries(autoclicker "-framework Cocoa" "-framework CoreGraphics")

else()
    # Linux - X11 needed for the global hotkey (XGrabKey) and, when
    # available, XTest for fast native click simulation. X11 itself is
    # required (the hotkey grab needs it); XTest is optional and clicker_engine
    # falls back to xdotool under pure Wayland sessions if it's missing.
    find_package(X11 REQUIRED)
    target_link_libraries(autoclicker ${X11_LIBRARIES})
    target_include_directories(autoclicker PRIVATE ${X11_INCLUDE_DIR})

    if(X11_Xtst_LIB)
        target_link_libraries(autoclicker ${X11_Xtst_LIB})
    endif()

    target_sources(autoclicker PRIVATE x11_hotkey.cpp x11_hotkey.h)
endif()

# Installation
install(TARGETS autoclicker DESTINATION bin)