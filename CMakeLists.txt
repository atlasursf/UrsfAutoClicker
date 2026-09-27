cmake_minimum_required(VERSION 3.21)
project(UrsfAutoClicker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    Network
    REQUIRED)

# Auto-generate MOC files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Frontend executable
add_executable(autoclicker-frontend
    frontend.cpp
    resources.qrc
)

if(UNIX AND NOT APPLE)
    target_sources(autoclicker-frontend PRIVATE x11_hotkey.cpp x11_hotkey.h)
endif()

target_link_libraries(autoclicker-frontend
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(UNIX AND NOT APPLE)
    # Linux - X11 needed by frontend for the global hotkey (XGrabKey)
    find_package(X11 REQUIRED)
    target_link_libraries(autoclicker-frontend ${X11_LIBRARIES})
    target_include_directories(autoclicker-frontend PRIVATE ${X11_INCLUDE_DIR})
endif()

# Backend executable (Wayland-compatible)
add_executable(clicker-backend
    backend_wayland.cpp
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(clicker-backend ws2_32)

elseif(APPLE)
    target_link_libraries(clicker-backend "-framework Cocoa" "-framework CoreGraphics" pthread)

else()
    # Linux - X11 and Wayland support
    find_package(X11 QUIET)

    if(X11_FOUND)
        # X11 is optional (may not be available on pure Wayland systems)
        target_link_libraries(clicker-backend ${X11_LIBRARIES})
        if(X11_Xtst_LIB)
            target_link_libraries(clicker-backend ${X11_Xtst_LIB})
        endif()
        target_include_directories(clicker-backend PRIVATE ${X11_INCLUDE_DIR})
        target_compile_definitions(clicker-backend PRIVATE HAVE_X11)
    endif()

    # Always link pthread
    target_link_libraries(clicker-backend pthread)

endif()

# Installation
install(TARGETS autoclicker-frontend clicker-backend DESTINATION bin)