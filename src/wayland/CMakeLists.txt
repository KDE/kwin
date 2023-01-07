add_subdirectory(tools)

if (BUILD_TESTING)
    add_subdirectory(autotests)
    add_subdirectory(tests)
endif()

# CMake has a known bug where target_source() doesn't work as expected with files generated
# in a directory other than the one where the target is defined. It should be fixed in 3.20.
add_library(WaylandProtocols_xml OBJECT)
set_property(TARGET WaylandProtocols_xml PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(WaylandProtocols_xml Qt::Core Wayland::Server)
target_link_libraries(kwin WaylandProtocols_xml)

ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${Wayland_DATADIR}/wayland.xml
    BASENAME wayland
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-device-v2.xml
    BASENAME kde-output-device-v2
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-management-v2.xml
    BASENAME kde-output-management-v2
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-shell.xml
    BASENAME plasma-shell
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-virtual-desktop.xml
    BASENAME org-kde-plasma-virtual-desktop
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-window-management.xml
    BASENAME plasma-window-management
)
ecm_add_wayland_server_protocol(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/surface-extension.xml
    BASENAME qt-surface-extension
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/idle.xml
    BASENAME idle
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml
    BASENAME fake-input
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/shadow.xml
    BASENAME shadow
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/dpms.xml
    BASENAME dpms
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/blur.xml
    BASENAME blur
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/contrast.xml
    BASENAME contrast
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    BASENAME relative-pointer-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/slide.xml
    BASENAME slide
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/server-decoration.xml
    BASENAME server-decoration
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml
    BASENAME text-input-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/text-input-unstable-v2.xml
    BASENAME text-input-unstable-v2
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
    BASENAME text-input-unstable-v3
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
    BASENAME pointer-gestures-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
    BASENAME pointer-constraints-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml
    BASENAME xdg-foreign-unstable-v2
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
    BASENAME idle-inhibit-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/appmenu.xml
    BASENAME appmenu
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/server-decoration-palette.xml
    BASENAME server-decoration-palette
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml
    BASENAME xdg-output-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
    BASENAME xdg-shell
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
    BASENAME xdg-decoration-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/keystate.xml
    BASENAME keystate
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
    BASENAME linux-dmabuf-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/tablet/tablet-unstable-v2.xml
    BASENAME tablet-unstable-v2
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wlr-data-control-unstable-v1.xml
    BASENAME wlr-data-control-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml
    BASENAME wlr-layer-shell-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
    BASENAME keyboard-shortcuts-inhibit-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
    BASENAME viewporter
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/primary-selection/primary-selection-unstable-v1.xml
    BASENAME wp-primary-selection-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml
    BASENAME zkde-screencast-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
    BASENAME input-method-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
    BASENAME xdg-activation-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/drm-lease/drm-lease-v1.xml
    BASENAME drm-lease-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-lockscreen-overlay-v1.xml
    BASENAME kde-lockscreen-overlay-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/ext-idle-notify/ext-idle-notify-v1.xml
    BASENAME ext-idle-notify-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/tearing-control/tearing-control-v1.xml
    BASENAME tearing-control-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xwayland-keyboard-grab/xwayland-keyboard-grab-unstable-v1.xml
    BASENAME xwayland-keyboard-grab-unstable-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/content-type/content-type-v1.xml
    BASENAME content-type-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/xwayland-shell/xwayland-shell-v1.xml
    BASENAME xwayland-shell-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-order-v1.xml
    BASENAME kde-output-order-v1
)

ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml
    BASENAME fractional-scale-v1
)

target_sources(kwin PRIVATE
    abstract_data_source.cpp
    abstract_drop_handler.cpp
    appmenu_interface.cpp
    blur_interface.cpp
    clientbuffer.cpp
    clientbufferintegration.cpp
    clientconnection.cpp
    compositor_interface.cpp
    contenttype_v1_interface.cpp
    contrast_interface.cpp
    datacontroldevice_v1_interface.cpp
    datacontroldevicemanager_v1_interface.cpp
    datacontroloffer_v1_interface.cpp
    datacontrolsource_v1_interface.cpp
    datadevice_interface.cpp
    datadevicemanager_interface.cpp
    dataoffer_interface.cpp
    datasource_interface.cpp
    display.cpp
    dpms_interface.cpp
    drmclientbuffer.cpp
    drmlease_v1_interface.cpp
    fakeinput_interface.cpp
    fractionalscale_v1_interface.cpp
    filtered_display.cpp
    idle_interface.cpp
    idleinhibit_v1_interface.cpp
    idlenotify_v1_interface.cpp
    inputmethod_v1_interface.cpp
    keyboard_interface.cpp
    keyboard_shortcuts_inhibit_v1_interface.cpp
    keystate_interface.cpp
    layershell_v1_interface.cpp
    linuxdmabufv1clientbuffer.cpp
    lockscreen_overlay_v1_interface.cpp
    output_interface.cpp
    output_order_v1_interface.cpp
    outputdevice_v2_interface.cpp
    outputmanagement_v2_interface.cpp
    plasmashell_interface.cpp
    plasmavirtualdesktop_interface.cpp
    plasmawindowmanagement_interface.cpp
    pointer_interface.cpp
    pointerconstraints_v1_interface.cpp
    pointergestures_v1_interface.cpp
    primaryselectiondevice_v1_interface.cpp
    primaryselectiondevicemanager_v1_interface.cpp
    primaryselectionoffer_v1_interface.cpp
    primaryselectionsource_v1_interface.cpp
    region_interface.cpp
    relativepointer_v1_interface.cpp
    screencast_v1_interface.cpp
    seat_interface.cpp
    server_decoration_interface.cpp
    server_decoration_palette_interface.cpp
    shadow_interface.cpp
    shmclientbuffer.cpp
    slide_interface.cpp
    subcompositor_interface.cpp
    surface_interface.cpp
    surfacerole.cpp
    tablet_v2_interface.cpp
    tearingcontrol_v1_interface.cpp
    textinput.cpp
    textinput_v1_interface.cpp
    textinput_v2_interface.cpp
    textinput_v3_interface.cpp
    touch_interface.cpp
    viewporter_interface.cpp
    xdgactivation_v1_interface.cpp
    xdgdecoration_v1_interface.cpp
    xdgforeign_v2_interface.cpp
    xdgoutput_v1_interface.cpp
    xdgshell_interface.cpp
    xwaylandkeyboardgrab_v1_interface.cpp
    xwaylandshell_v1_interface.cpp
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_sources(kwin PRIVATE utils/executable_path_proc.cpp)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    target_sources(kwin PRIVATE utils/executable_path_sysctl.cpp)
else()
    message(FATAL_ERROR "Unsupported platform ${CMAKE_SYSTEM_NAME}")
endif()
