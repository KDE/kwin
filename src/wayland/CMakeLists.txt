add_subdirectory(tools)

# CMake has a known bug where target_source() doesn't work as expected with files generated
# in a directory other than the one where the target is defined. It should be fixed in 3.20.
add_library(WaylandProtocols_xml OBJECT)
set_property(TARGET WaylandProtocols_xml PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(WaylandProtocols_xml Qt::Core Wayland::Server)
target_link_libraries(kwin PRIVATE WaylandProtocols_xml)

ecm_add_qtwayland_server_protocol_kde(WaylandProtocols_xml
    PRIVATE_CODE
    FILES
        ${Wayland_DATADIR}/wayland.xml

        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/appmenu.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/blur.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/contrast.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/dpms.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/idle.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-external-brightness-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-lockscreen-overlay-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-device-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-management-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-order-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-screen-edge-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/keystate.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/org-kde-plasma-virtual-desktop.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-shell.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-window-management.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/server-decoration-palette.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/server-decoration.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/shadow.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/slide.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/text-input-unstable-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml

        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/drm.xml
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/frog-color-management-v1.xml
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/xx-pip-v1.xml
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/xx-session-management-v1.xml

        ${WaylandProtocols_DATADIR}/stable/presentation-time/presentation-time.xml
        ${WaylandProtocols_DATADIR}/stable/tablet/tablet-v2.xml
        ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
        ${WaylandProtocols_DATADIR}/staging/alpha-modifier/alpha-modifier-v1.xml
        ${WaylandProtocols_DATADIR}/staging/color-management/color-management-v1.xml
        ${WaylandProtocols_DATADIR}/staging/color-representation/color-representation-v1.xml
        ${WaylandProtocols_DATADIR}/staging/content-type/content-type-v1.xml
        ${WaylandProtocols_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml
        ${WaylandProtocols_DATADIR}/staging/drm-lease/drm-lease-v1.xml
        ${WaylandProtocols_DATADIR}/staging/ext-data-control/ext-data-control-v1.xml
        ${WaylandProtocols_DATADIR}/staging/ext-idle-notify/ext-idle-notify-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fifo/fifo-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml
        ${WaylandProtocols_DATADIR}/staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml
        ${WaylandProtocols_DATADIR}/staging/security-context/security-context-v1.xml
        ${WaylandProtocols_DATADIR}/staging/single-pixel-buffer/single-pixel-buffer-v1.xml
        ${WaylandProtocols_DATADIR}/staging/tearing-control/tearing-control-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-system-bell/xdg-system-bell-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-toplevel-drag/xdg-toplevel-drag-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-toplevel-tag/xdg-toplevel-tag-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xwayland-shell/xwayland-shell-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/primary-selection/primary-selection-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
        ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml
        ${WaylandProtocols_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/xwayland-keyboard-grab/xwayland-keyboard-grab-unstable-v1.xml
)

target_sources(kwin PRIVATE
    abstract_data_source.cpp
    abstract_drop_handler.cpp
    alphamodifier_v1.cpp
    appmenu.cpp
    blur.cpp
    clientconnection.cpp
    colormanagement_v1.cpp
    colorrepresentation_v1.cpp
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
    externalbrightness_v1.cpp
    fifo_v1.cpp
    filtered_display.cpp
    fixes.cpp
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
    linux_drm_syncobj_v1.cpp
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
    singlepixelbuffer.cpp
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
    xdgdialog_v1.cpp
    xdgforeign_v2.cpp
    xdgoutput_v1.cpp
    xdgsession_v1.cpp
    xdgshell.cpp
    xdgsystembell_v1.cpp
    xdgtopleveldrag_v1.cpp
    xdgtoplevelicon_v1.cpp
    xdgtopleveltag_v1.cpp
    xwaylandkeyboardgrab_v1.cpp
    xwaylandshell_v1.cpp
    xxpip_v1.cpp
)

install(FILES
    alphamodifier_v1.h
    appmenu.h
    blur.h
    clientconnection.h
    colormanagement_v1.h
    colorrepresentation_v1.h
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
    externalbrightness_v1.h
    fifo_v1.h
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
    linux_drm_syncobj_v1.h
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
    singlepixelbuffer.h
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
    xdgdialog_v1.h
    xdgforeign_v2.h
    xdgoutput_v1.h
    xdgsession_v1.h
    xdgshell.h
    xdgsystembell_v1.h
    xdgtoplevelicon_v1.h
    xdgtopleveltag_v1.h
    xwaylandkeyboardgrab_v1.h
    xwaylandshell_v1.h

    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-alpha-modifier-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-color-management-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-color-representation-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-content-type-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-fifo-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-frog-color-management-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-kde-external-brightness-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-linux-drm-syncobj-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-presentation-time.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-single-pixel-buffer-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-xdg-toplevel-tag-v1.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-alpha-modifier-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-color-management-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-color-representation-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-content-type-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fifo-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-frog-color-management-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-kde-external-brightness-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-linux-drm-syncobj-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-presentation-time-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-single-pixel-buffer-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-toplevel-tag-v1-server-protocol.h

    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kwin/wayland COMPONENT Devel)
