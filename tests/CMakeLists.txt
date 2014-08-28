if (XCB_ICCCM_FOUND)
    set(normalhintsbasesizetest_SRCS normalhintsbasesizetest.cpp)
    add_executable(normalhintsbasesizetest ${normalhintsbasesizetest_SRCS})
    target_link_libraries(normalhintsbasesizetest XCB::XCB XCB::ICCCM KF5::WindowSystem)
endif()

# next target
set(screenedgeshowtest_SRCS screenedgeshowtest.cpp)
add_executable(screenedgeshowtest ${screenedgeshowtest_SRCS})
target_link_libraries(screenedgeshowtest Qt5::Widgets Qt5::X11Extras KF5::WindowSystem ${XCB_XCB_LIBRARY})

if (Wayland_Client_FOUND AND XKB_FOUND)
    add_definitions(-DSOURCE_DIR="${KWIN_SOURCE_DIR}")
    include_directories(${CMAKE_BINARY_DIR}/wayland_protocols/)
    set_source_files_properties(${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c GENERATED)
    set(waylandclienttest_SRCS
            waylandclienttest.cpp
            ../wayland_client/buffer.cpp
            ../wayland_client/compositor.cpp
            ../wayland_client/connection_thread.cpp
            ../wayland_client/keyboard.cpp
            ../wayland_client/output.cpp
            ../wayland_client/pointer.cpp
            ../wayland_client/registry.cpp
            ../wayland_client/seat.cpp
            ../wayland_client/shell.cpp
            ../wayland_client/shm_pool.cpp
            ../wayland_client/surface.cpp
            ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
       )
    add_executable(waylandclienttest ${waylandclienttest_SRCS})
    target_link_libraries(waylandclienttest Qt5::Core Qt5::Gui Wayland::Client)
    add_dependencies(waylandclienttest wayland-client-fullscreen-shell)
endif()
