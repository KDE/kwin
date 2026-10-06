add_subdirectory(helper)

add_library(KWinIntegrationTestFramework STATIC)

qt6_generate_wayland_protocol_client_sources(KWinIntegrationTestFramework
    PRIVATE_CODE
    NO_INCLUDE_CORE_ONLY
    FILES
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
)
qt6_generate_wayland_protocol_client_sources(KWinIntegrationTestFramework
    PRIVATE_CODE
    FILES
        ${Wayland_DATADIR}/wayland.xml

        ${CMAKE_SOURCE_DIR}/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml
        ${CMAKE_SOURCE_DIR}/src/wayland/protocols/xx-session-management-v1.xml
        ${WaylandProtocols_DATADIR}/stable/presentation-time/presentation-time.xml
        ${WaylandProtocols_DATADIR}/stable/tablet/tablet-v2.xml
        ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
        ${WaylandProtocols_DATADIR}/staging/color-management/color-management-v1.xml
        ${WaylandProtocols_DATADIR}/staging/color-representation/color-representation-v1.xml
        ${WaylandProtocols_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fifo/fifo-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml
        ${WaylandProtocols_DATADIR}/staging/security-context/security-context-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-toplevel-drag/xdg-toplevel-drag-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/primary-selection/primary-selection-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
        ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-device-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-management-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-screen-edge-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/keystate.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml
)
target_sources(KWinIntegrationTestFramework PRIVATE
    generic_scene_opengl_test.cpp
    kwin_wayland_test.cpp
    test_helpers.cpp
)
target_link_libraries(KWinIntegrationTestFramework
    PUBLIC
        Qt::Test
        Qt::Concurrent
        Plasma::KWaylandClient
        Wayland::Client
        Libdrm::Libdrm
        kwin
        XKB::XKB

    PRIVATE
        # Static plugins
        KWinQpaPlugin
        KF6WindowSystemKWinPlugin
        KF6IdleTimeKWinPlugin
)
if(KWIN_BUILD_X11)
    target_link_libraries(KWinIntegrationTestFramework PRIVATE KWinXwaylandServerModule)
endif()
if(TARGET KF6GlobalAccelKWinPlugin)
    target_link_libraries(KWinIntegrationTestFramework PUBLIC KF6GlobalAccelKWinPlugin)
endif()

if(TARGET PW::KScreenLocker)
    target_link_libraries(KWinIntegrationTestFramework PUBLIC PW::KScreenLocker)
endif()

function(integrationTest)
    set(optionArgs BUILTIN_EFFECTS)
    set(oneValueArgs NAME)
    set(multiValueArgs SRCS LIBS OPTIONAL_LIBS)
    cmake_parse_arguments(ARGS "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${ARGS_NAME} ${ARGS_SRCS})
    target_link_libraries(${ARGS_NAME} KWinIntegrationTestFramework Qt::Test ${ARGS_LIBS})
    foreach(target ${ARGS_OPTIONAL_LIBS})
        if(TARGET ${target})
            target_link_libraries(${ARGS_NAME} ${target})
        endif()
    endforeach()
    if(${ARGS_BUILTIN_EFFECTS})
        kcoreaddons_target_static_plugins(${ARGS_NAME} NAMESPACE "kwin/effects/plugins")
    endif()
    add_test(NAME kwin-${ARGS_NAME} COMMAND dbus-run-session ${CMAKE_BINARY_DIR}/bin/${ARGS_NAME})
endfunction()

integrationTest(NAME testXdgShellWindow SRCS xdgshellwindow_test.cpp LIBS KDecoration3::KDecoration)

add_subdirectory(fakes)
