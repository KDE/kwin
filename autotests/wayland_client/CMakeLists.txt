include_directories(${CMAKE_BINARY_DIR}/wayland_protocols/)
set_source_files_properties(${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c GENERATED)
########################################################
# Test WaylandConnectionThread
########################################################
set( testWaylandConnectionThread_SRCS test_wayland_connection_thread.cpp ${KWIN_SOURCE_DIR}/wayland_client/connection_thread.cpp )
add_executable(testWaylandConnectionThread ${testWaylandConnectionThread_SRCS})
target_link_libraries( testWaylandConnectionThread Qt5::Test Wayland::Client)
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
    )
add_executable(testWaylandOutput ${testWaylandOutput_SRCS})
add_dependencies(testWaylandOutput wayland-client-fullscreen-shell)
target_link_libraries( testWaylandOutput Qt5::Test Wayland::Client)
add_test(kwin-testWaylandOutput testWaylandOutput)
ecm_mark_as_test(testWaylandOutput)
