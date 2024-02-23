add_subdirectory(tools)

# CMake has a known bug where target_source() doesn't work as expected with files generated
# in a directory other than the one where the target is defined. It should be fixed in 3.20.
add_library(WaylandProtocols_xml OBJECT)
set_property(TARGET WaylandProtocols_xml PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(WaylandProtocols_xml Qt::Core Wayland::Server)
target_link_libraries(kwin PRIVATE WaylandProtocols_xml)

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
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wayland-drm.xml
    BASENAME drm
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-screen-edge-v1.xml
    BASENAME kde-screen-edge-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml
    BASENAME cursor-shape-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL protocols/xdg-toplevel-drag-v1.xml
    BASENAME xdg-toplevel-drag-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/wayland/protocols/frog-color-management-v1.xml
    BASENAME frog-color-management-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/presentation-time/presentation-time.xml
    BASENAME presentation-time
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/security-context/security-context-v1.xml
    BASENAME security-context-v1
)
ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/wayland/protocols/xx-color-management-v2.xml
    BASENAME xx-color-management-v2
)

target_sources(kwin PRIVATE
    abstract_data_source.cpp
    abstract_drop_handler.cpp
    appmenu.cpp
    blur.cpp
    clientconnection.cpp
    compositor.cpp
    contenttype_v1.cpp
    contrast.cpp
    cursorshape_v1.cpp
    datacontroldevice_v1.cpp
    datacontroldevicemanager_v1.cpp
    datacontroloffer_v1.cpp
    datacontrolsource_v1.cpp
    datadevice.cpp
    datadevicemanager.cpp
    dataoffer.cpp
    datasource.cpp
    display.cpp
    dpms.cpp
    drmclientbuffer.cpp
    drmlease_v1.cpp
    filtered_display.cpp
    fractionalscale_v1.cpp
    frog_colormanagement_v1.cpp
    idle.cpp
    idleinhibit_v1.cpp
    idlenotify_v1.cpp
    inputmethod_v1.cpp
    keyboard.cpp
    keyboard_shortcuts_inhibit_v1.cpp
    keystate.cpp
    layershell_v1.cpp
    linuxdmabufv1clientbuffer.cpp
    lockscreen_overlay_v1.cpp
    output.cpp
    output_order_v1.cpp
    outputdevice_v2.cpp
    outputmanagement_v2.cpp
    plasmashell.cpp
    plasmavirtualdesktop.cpp
    plasmawindowmanagement.cpp
    pointer.cpp
    pointerconstraints_v1.cpp
    pointergestures_v1.cpp
    presentationtime.cpp
    primaryselectiondevice_v1.cpp
    primaryselectiondevicemanager_v1.cpp
    primaryselectionoffer_v1.cpp
    primaryselectionsource_v1.cpp
    region.cpp
    relativepointer_v1.cpp
    screencast_v1.cpp
    screenedge_v1.cpp
    seat.cpp
    securitycontext_v1.cpp
    server_decoration.cpp
    server_decoration_palette.cpp
    shadow.cpp
    shmclientbuffer.cpp
    slide.cpp
    subcompositor.cpp
    surface.cpp
    tablet_v2.cpp
    tearingcontrol_v1.cpp
    textinput.cpp
    textinput_v1.cpp
    textinput_v2.cpp
    textinput_v3.cpp
    touch.cpp
    transaction.cpp
    viewporter.cpp
    xdgactivation_v1.cpp
    xdgdecoration_v1.cpp
    xdgforeign_v2.cpp
    xdgoutput_v1.cpp
    xdgshell.cpp
    xdgtopleveldrag_v1.cpp
    xwaylandkeyboardgrab_v1.cpp
    xwaylandshell_v1.cpp
    xx_colormanagement_v2.cpp
)

install(FILES
    appmenu.h
    blur.h
    clientconnection.h
    compositor.h
    contenttype_v1.h
    contrast.h
    cursorshape_v1.h
    datacontroldevice_v1.h
    datacontroldevicemanager_v1.h
    datacontroloffer_v1.h
    datacontrolsource_v1.h
    datadevice.h
    datadevicemanager.h
    dataoffer.h
    datasource.h
    display.h
    dpms.h
    drmlease_v1.h
    fractionalscale_v1.h
    frog_colormanagement_v1.h
    idle.h
    idleinhibit_v1.h
    idlenotify_v1.h
    inputmethod_v1.h
    keyboard.h
    keyboard_shortcuts_inhibit_v1.h
    keystate.h
    layershell_v1.h
    lockscreen_overlay_v1.h
    output.h
    output_order_v1.h
    outputdevice_v2.h
    outputmanagement_v2.h
    plasmashell.h
    plasmavirtualdesktop.h
    plasmawindowmanagement.h
    pointer.h
    pointerconstraints_v1.h
    pointergestures_v1.h
    presentationtime.h
    primaryselectiondevice_v1.h
    primaryselectiondevicemanager_v1.h
    primaryselectionoffer_v1.h
    primaryselectionsource_v1.h
    quirks.h
    relativepointer_v1.h
    screencast_v1.h
    screenedge_v1.h
    seat.h
    securitycontext_v1.h
    server_decoration.h
    server_decoration_palette.h
    shadow.h
    slide.h
    subcompositor.h
    surface.h
    tablet_v2.h
    tearingcontrol_v1.h
    textinput_v1.h
    textinput_v2.h
    textinput_v3.h
    touch.h
    viewporter.h
    xdgactivation_v1.h
    xdgdecoration_v1.h
    xdgforeign_v2.h
    xdgoutput_v1.h
    xdgshell.h
    xwaylandkeyboardgrab_v1.h
    xwaylandshell_v1.h
    xx_colormanagement_v2.h

    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-content-type-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-frog-color-management-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-presentation-time.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-xx-color-management-v2.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-content-type-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-frog-color-management-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-presentation-time-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xx-color-management-v2-server-protocol.h

    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/wayland COMPONENT Devel)
