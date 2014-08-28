########################################################
# Test WaylandServerDisplay
########################################################
set( testWaylandServerDisplay_SRCS
        test_display.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/buffer_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/compositor_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/display.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/output_interface.cpp
        ${KWIN_SOURCE_DIR}/wayland_server/surface_interface.cpp
    )
add_executable(testWaylandServerDisplay ${testWaylandServerDisplay_SRCS})
target_link_libraries( testWaylandServerDisplay Qt5::Test Qt5::Gui Wayland::Server)
add_test(kwin-testWaylandServerDisplay testWaylandServerDisplay)
ecm_mark_as_test(testWaylandServerDisplay)
