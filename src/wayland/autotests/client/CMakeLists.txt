include_directories(${CMAKE_BINARY_DIR}/wayland_protocols/)
set_source_files_properties(${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c GENERATED)
########################################################
# Test WaylandConnectionThread
########################################################
set( testWaylandConnectionThread_SRCS
        test_wayland_connection_thread.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/buffer_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/connection_thread.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/compositor_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/output_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/seat_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/shell_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/surface_interface.cpp
    )
add_executable(testWaylandConnectionThread ${testWaylandConnectionThread_SRCS})
target_link_libraries( testWaylandConnectionThread Qt5::Test Qt5::Gui Wayland::Client Wayland::Server)
add_test(kwin-testWaylandConnectionThread testWaylandConnectionThread)
ecm_mark_as_test(testWaylandConnectionThread)

########################################################
# Test WaylandRegistry
########################################################
set( testWaylandRegistry_SRCS
        test_wayland_registry.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/connection_thread.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/registry.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
        ${KWAYLAND_SOURCE_DIR}/src/server/buffer_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/compositor_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/output_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/seat_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/shell_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/surface_interface.cpp
    )
add_executable(testWaylandRegistry ${testWaylandRegistry_SRCS})
add_dependencies(testWaylandRegistry wayland-client-fullscreen-shell)
target_link_libraries( testWaylandRegistry Qt5::Test Qt5::Gui Wayland::Client Wayland::Server)
add_test(kwin-testWaylandRegistry testWaylandRegistry)
ecm_mark_as_test(testWaylandRegistry)

########################################################
# Test WaylandFullscreenShell
########################################################
if(Wayland_VERSION VERSION_GREATER "1.4.0")
    set( testWaylandFullscreenShell_SRCS
            test_wayland_fullscreen_shell.cpp
            ${KWAYLAND_SOURCE_DIR}/src/client/connection_thread.cpp
            ${KWAYLAND_SOURCE_DIR}/src/client/registry.cpp
            ${KWAYLAND_SOURCE_DIR}/src/client/fullscreen_shell.cpp
            ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
        )
    add_executable(testWaylandFullscreenShell ${testWaylandFullscreenShell_SRCS})
    add_dependencies(testWaylandFullscreenShell wayland-client-fullscreen-shell)
    target_link_libraries( testWaylandFullscreenShell Qt5::Test Wayland::Client)
    add_test(kwin-testWaylandFullscreenShell testWaylandFullscreenShell)
    ecm_mark_as_test(testWaylandFullscreenShell)
endif()

########################################################
# Test WaylandOutput
########################################################
set( testWaylandOutput_SRCS
        test_wayland_output.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/connection_thread.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/registry.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/fullscreen_shell.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/output.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
        ${KWAYLAND_SOURCE_DIR}/src/server/buffer_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/compositor_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/output_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/seat_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/shell_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/surface_interface.cpp
    )
add_executable(testWaylandOutput ${testWaylandOutput_SRCS})
add_dependencies(testWaylandOutput wayland-client-fullscreen-shell)
target_link_libraries( testWaylandOutput Qt5::Test Qt5::Gui Wayland::Client Wayland::Server)
add_test(kwin-testWaylandOutput testWaylandOutput)
ecm_mark_as_test(testWaylandOutput)

########################################################
# Test WaylandShell
########################################################
set( testWaylandShell_SRCS
        test_wayland_shell.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/compositor.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/connection_thread.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/registry.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/fullscreen_shell.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/shell.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/surface.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
        ${KWAYLAND_SOURCE_DIR}/src/server/buffer_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/compositor_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/output_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/seat_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/shell_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/surface_interface.cpp
    )
add_executable(testWaylandShell ${testWaylandShell_SRCS})
add_dependencies(testWaylandShell wayland-client-fullscreen-shell)
target_link_libraries( testWaylandShell Qt5::Test Qt5::Gui Wayland::Client Wayland::Server)
add_test(kwin-testWaylandShell testWaylandShell)
ecm_mark_as_test(testWaylandShell)

########################################################
# Test WaylandSurface
########################################################
set( testWaylandSurface_SRCS
        test_wayland_surface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/buffer.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/compositor.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/connection_thread.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/registry.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/fullscreen_shell.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/shm_pool.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/surface.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
        ${KWAYLAND_SOURCE_DIR}/src/server/buffer_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/compositor_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/output_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/seat_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/shell_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/surface_interface.cpp
    )
add_executable(testWaylandSurface ${testWaylandSurface_SRCS})
add_dependencies(testWaylandSurface wayland-client-fullscreen-shell)
target_link_libraries( testWaylandSurface Qt5::Test Qt5::Gui Wayland::Client Wayland::Server)
add_test(kwin-testWaylandSurface testWaylandSurface)
ecm_mark_as_test(testWaylandSurface)

########################################################
# Test WaylandSeat
########################################################
set( testWaylandSeat_SRCS
        test_wayland_seat.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/buffer.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/compositor.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/connection_thread.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/registry.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/fullscreen_shell.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/keyboard.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/pointer.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/seat.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/shm_pool.cpp
        ${KWAYLAND_SOURCE_DIR}/src/client/surface.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
        ${KWAYLAND_SOURCE_DIR}/src/server/buffer_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/compositor_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/display.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/output_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/seat_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/shell_interface.cpp
        ${KWAYLAND_SOURCE_DIR}/src/server/surface_interface.cpp
    )
add_executable(testWaylandSeat ${testWaylandSeat_SRCS})
add_dependencies(testWaylandSeat wayland-client-fullscreen-shell)
target_link_libraries( testWaylandSeat Qt5::Test Qt5::Gui Wayland::Client Wayland::Server)
add_test(kwin-testWaylandSeat testWaylandSeat)
ecm_mark_as_test(testWaylandSeat)
