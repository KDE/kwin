include_directories(${CMAKE_BINARY_DIR}/wayland_protocols/)
set_source_files_properties(${CMAKE_BINARY_DIR}/wayland_protocols/wayland-client-fullscreen-shell.c GENERATED)
########################################################
# Test WaylandConnectionThread
########################################################
set( testWaylandConnectionThread_SRCS
        test_wayland_connection_thread.cpp
    )
add_executable(testWaylandConnectionThread ${testWaylandConnectionThread_SRCS})
target_link_libraries( testWaylandConnectionThread Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwin-testWaylandConnectionThread testWaylandConnectionThread)
ecm_mark_as_test(testWaylandConnectionThread)

########################################################
# Test WaylandRegistry
########################################################
set( testWaylandRegistry_SRCS
        test_wayland_registry.cpp
    )
add_executable(testWaylandRegistry ${testWaylandRegistry_SRCS})
target_link_libraries( testWaylandRegistry Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwin-testWaylandRegistry testWaylandRegistry)
ecm_mark_as_test(testWaylandRegistry)

########################################################
# Test WaylandFullscreenShell
########################################################
if(Wayland_VERSION VERSION_GREATER "1.4.0")
    set( testWaylandFullscreenShell_SRCS
            test_wayland_fullscreen_shell.cpp
        )
    add_executable(testWaylandFullscreenShell ${testWaylandFullscreenShell_SRCS})
    target_link_libraries( testWaylandFullscreenShell Qt5::Test KF5::WaylandClient Wayland::Client)
    add_test(kwin-testWaylandFullscreenShell testWaylandFullscreenShell)
    ecm_mark_as_test(testWaylandFullscreenShell)
endif()

########################################################
# Test WaylandOutput
########################################################
set( testWaylandOutput_SRCS
        test_wayland_output.cpp
    )
add_executable(testWaylandOutput ${testWaylandOutput_SRCS})
target_link_libraries( testWaylandOutput Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwin-testWaylandOutput testWaylandOutput)
ecm_mark_as_test(testWaylandOutput)

########################################################
# Test WaylandShell
########################################################
set( testWaylandShell_SRCS
        test_wayland_shell.cpp
    )
add_executable(testWaylandShell ${testWaylandShell_SRCS})
target_link_libraries( testWaylandShell Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwin-testWaylandShell testWaylandShell)
ecm_mark_as_test(testWaylandShell)

########################################################
# Test WaylandSurface
########################################################
set( testWaylandSurface_SRCS
        test_wayland_surface.cpp
    )
add_executable(testWaylandSurface ${testWaylandSurface_SRCS})
target_link_libraries( testWaylandSurface Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwin-testWaylandSurface testWaylandSurface)
ecm_mark_as_test(testWaylandSurface)

########################################################
# Test WaylandSeat
########################################################
set( testWaylandSeat_SRCS
        test_wayland_seat.cpp
    )
add_executable(testWaylandSeat ${testWaylandSeat_SRCS})
target_link_libraries( testWaylandSeat Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwin-testWaylandSeat testWaylandSeat)
ecm_mark_as_test(testWaylandSeat)
