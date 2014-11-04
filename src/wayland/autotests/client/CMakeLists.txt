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
add_test(kwayland-testWaylandConnectionThread testWaylandConnectionThread)
ecm_mark_as_test(testWaylandConnectionThread)

########################################################
# Test WaylandRegistry
########################################################
set( testWaylandRegistry_SRCS
        test_wayland_registry.cpp
    )
add_executable(testWaylandRegistry ${testWaylandRegistry_SRCS})
target_link_libraries( testWaylandRegistry Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwayland-testWaylandRegistry testWaylandRegistry)
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
    add_test(kwayland-testWaylandFullscreenShell testWaylandFullscreenShell)
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
add_test(kwayland-testWaylandOutput testWaylandOutput)
ecm_mark_as_test(testWaylandOutput)

########################################################
# Test WaylandShell
########################################################
set( testWaylandShell_SRCS
        test_wayland_shell.cpp
    )
add_executable(testWaylandShell ${testWaylandShell_SRCS})
target_link_libraries( testWaylandShell Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwayland-testWaylandShell testWaylandShell)
ecm_mark_as_test(testWaylandShell)

########################################################
# Test WaylandSurface
########################################################
set( testWaylandSurface_SRCS
        test_wayland_surface.cpp
    )
add_executable(testWaylandSurface ${testWaylandSurface_SRCS})
target_link_libraries( testWaylandSurface Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwayland-testWaylandSurface testWaylandSurface)
ecm_mark_as_test(testWaylandSurface)

########################################################
# Test WaylandSeat
########################################################
set( testWaylandSeat_SRCS
        test_wayland_seat.cpp
    )
add_executable(testWaylandSeat ${testWaylandSeat_SRCS})
target_link_libraries( testWaylandSeat Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(kwayland-testWaylandSeat testWaylandSeat)
ecm_mark_as_test(testWaylandSeat)

########################################################
# Test ShmPool
########################################################
set( testShmPool_SRCS
        test_shm_pool.cpp
    )
add_executable(testShmPool ${testShmPool_SRCS})
target_link_libraries( testShmPool Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(kwayland-testShmPool testShmPool)
ecm_mark_as_test(testShmPool)

########################################################
# Test Compositor
########################################################
set( testCompositor_SRCS
        test_compositor.cpp
    )
add_executable(testCompositor ${testCompositor_SRCS})
target_link_libraries( testCompositor Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(kwayland-testCompositor testCompositor)
ecm_mark_as_test(testCompositor)

########################################################
# Test SubCompositor
########################################################
set( testSubCompositor_SRCS
        test_wayland_subcompositor.cpp
    )
add_executable(testSubCompositor ${testSubCompositor_SRCS})
target_link_libraries( testSubCompositor Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(kwayland-testSubCompositor testSubCompositor)
ecm_mark_as_test(testSubCompositor)


########################################################
# Test SubSurface
########################################################
set( testSubSurface_SRCS
        test_wayland_subsurface.cpp
    )
add_executable(testSubSurface ${testSubSurface_SRCS})
target_link_libraries( testSubSurface Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(kwayland-testSubSurface testSubSurface)
ecm_mark_as_test(testSubSurface)

########################################################
# Test Region
########################################################
set( testRegion_SRCS
        test_wayland_region.cpp
    )
add_executable(testRegion ${testRegion_SRCS})
target_link_libraries( testRegion Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(kwayland-testRegion testRegion)
ecm_mark_as_test(testRegion)

########################################################
# Test DataSource
########################################################
set( testDataSource_SRCS
        test_datasource.cpp
    )
add_executable(testDataSource ${testDataSource_SRCS})
target_link_libraries( testDataSource Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(kwayland-testDataSource testDataSource)
ecm_mark_as_test(testDataSource)
