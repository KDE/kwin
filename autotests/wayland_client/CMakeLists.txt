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
    )
add_executable(testWaylandRegistry ${testWaylandRegistry_SRCS})
target_link_libraries( testWaylandRegistry Qt5::Test Wayland::Client)
add_test(kwin-testWaylandRegistry testWaylandRegistry)
ecm_mark_as_test(testWaylandRegistry)
