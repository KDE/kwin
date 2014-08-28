include_directories(${CMAKE_BINARY_DIR}/wayland_protocols/)
set_source_files_properties(${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c GENERATED)
########################################################
# Test WaylandConnectionThread
########################################################
set( testWaylandConnectionThread_SRCS
        test_wayland_connection_thread.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/buffer_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/connection_thread.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/compositor_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/display.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/output_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/surface_interface.cpp
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
        ${KWIN_SOURCE_DIR}/wayland_client/connection_thread.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/registry.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
    )
add_executable(testWaylandRegistry ${testWaylandRegistry_SRCS})
add_dependencies(testWaylandRegistry wayland-client-fullscreen-shell)
target_link_libraries( testWaylandRegistry Qt5::Test Wayland::Client)
add_test(kwin-testWaylandRegistry testWaylandRegistry)
ecm_mark_as_test(testWaylandRegistry)

########################################################
# Test WaylandFullscreenShell
########################################################
if(Wayland_VERSION VERSION_GREATER "1.4.0")
    set( testWaylandFullscreenShell_SRCS
            test_wayland_fullscreen_shell.cpp
            ${KWIN_SOURCE_DIR}/wayland_client/connection_thread.cpp
            ${KWIN_SOURCE_DIR}/wayland_client/registry.cpp
            ${KWIN_SOURCE_DIR}/wayland_client/fullscreen_shell.cpp
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
        ${KWIN_SOURCE_DIR}/wayland_client/connection_thread.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/registry.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/fullscreen_shell.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/output.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
        ${KWIN_SOURCE_DIR}/wayland_server/buffer_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/compositor_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/display.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/output_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/surface_interface.cpp
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
        ${KWIN_SOURCE_DIR}/wayland_client/compositor.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/connection_thread.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/registry.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/fullscreen_shell.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/shell.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/surface.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
    )
add_executable(testWaylandShell ${testWaylandShell_SRCS})
add_dependencies(testWaylandShell wayland-client-fullscreen-shell)
target_link_libraries( testWaylandShell Qt5::Test Qt5::Gui Wayland::Client)
add_test(kwin-testWaylandShell testWaylandShell)
ecm_mark_as_test(testWaylandShell)

########################################################
# Test WaylandSurface
########################################################
set( testWaylandSurface_SRCS
        test_wayland_surface.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/buffer.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/compositor.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/connection_thread.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/registry.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/fullscreen_shell.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/shm_pool.cpp
        ${KWIN_SOURCE_DIR}/wayland_client/surface.cpp
        ${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c
        ${KWIN_SOURCE_DIR}/wayland_server/buffer_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/compositor_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/display.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/output_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/surface_interface.cpp
    )
add_executable(testWaylandSurface ${testWaylandSurface_SRCS})
add_dependencies(testWaylandSurface wayland-client-fullscreen-shell)
target_link_libraries( testWaylandSurface Qt5::Test Qt5::Gui Wayland::Client Wayland::Server)
add_test(kwin-testWaylandSurface testWaylandSurface)
ecm_mark_as_test(testWaylandSurface)
