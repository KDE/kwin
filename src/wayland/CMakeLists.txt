set(SERVER_LIB_SRCS
    abstract_data_source.cpp
    appmenu_interface.cpp
    blur_interface.cpp
    buffer_interface.cpp
    clientconnection.cpp
    compositor_interface.cpp
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
    eglstream_controller_interface.cpp
    fakeinput_interface.cpp
    filtered_display.cpp
    global.cpp
    idle_interface.cpp
    idleinhibit_interface.cpp
    idleinhibit_interface_v1.cpp
    keyboard_interface.cpp
    keyboard_shortcuts_inhibit_v1_interface.cpp
    keystate_interface.cpp
    linuxdmabuf_v1_interface.cpp
    output_interface.cpp
    outputchangeset.cpp
    outputconfiguration_interface.cpp
    outputdevice_interface.cpp
    outputmanagement_interface.cpp
    plasmashell_interface.cpp
    plasmavirtualdesktop_interface.cpp
    plasmawindowmanagement_interface.cpp
    pointer_interface.cpp
    pointerconstraints_interface.cpp
    pointerconstraints_interface_v1.cpp
    pointergestures_interface.cpp
    pointergestures_interface_v1.cpp
    region_interface.cpp
    relativepointer_interface.cpp
    relativepointer_interface_v1.cpp
    remote_access_interface.cpp
    resource.cpp
    seat_interface.cpp
    server_decoration_interface.cpp
    server_decoration_palette_interface.cpp
    shadow_interface.cpp
    slide_interface.cpp
    subcompositor_interface.cpp
    surface_interface.cpp
    surfacerole.cpp
    tablet_interface.cpp
    textinput_interface.cpp
    textinput_interface_v0.cpp
    textinput_interface_v2.cpp
    touch_interface.cpp
    xdgdecoration_interface.cpp
    xdgforeign_interface.cpp
    xdgforeign_v2_interface.cpp
    xdgoutput_interface.cpp
    xdgshell_interface.cpp
    xdgshell_stable_interface.cpp
    xdgshell_v6_interface.cpp
)

ecm_qt_declare_logging_category(SERVER_LIB_SRCS
    HEADER logging.h
    IDENTIFIER KWAYLAND_SERVER
    CATEGORY_NAME kwayland-server
    DEFAULT_SEVERITY Critical
    DESCRIPTION "KWayland Server Library"
    EXPORT KWAYLAND
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/output-management.xml
    BASENAME output-management
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/outputdevice.xml
    BASENAME org_kde_kwin_outputdevice
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-shell.xml
    BASENAME plasma-shell
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-virtual-desktop.xml
    BASENAME plasma-virtual-desktop
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-window-management.xml
    BASENAME plasma-window-management
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/surface-extension.xml
    BASENAME qt-surface-extension
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/idle.xml
    BASENAME idle
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml
    BASENAME fake-input
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/shadow.xml
    BASENAME shadow
)
ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/dpms.xml
    BASENAME dpms
)

ecm_add_qtwayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/blur.xml
    BASENAME blur
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/contrast.xml
    BASENAME contrast
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    BASENAME relativepointer-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/slide.xml
    BASENAME slide
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/server-decoration.xml
    BASENAME server_decoration
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/text-input.xml
    BASENAME text
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/text-input-unstable-v2.xml
    BASENAME text-input-unstable-v2
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml
    BASENAME xdg-shell-v6
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
    BASENAME pointer-gestures-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
    BASENAME pointer-constraints-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml
    BASENAME xdg-foreign-unstable-v2
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
    BASENAME idle-inhibit-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/appmenu.xml
    BASENAME appmenu
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/server-decoration-palette.xml
    BASENAME server_decoration_palette
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/remote-access.xml
    BASENAME remote-access
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml
    BASENAME xdg-output
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
    BASENAME xdg-shell
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
    BASENAME xdg-decoration
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/wayland-eglstream-controller.xml
    BASENAME eglstream-controller
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/keystate.xml
    BASENAME keystate
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
    BASENAME linux-dmabuf-unstable-v1
)

ecm_add_qtwayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/tablet/tablet-unstable-v2.xml
    BASENAME tablet-unstable-v2
)

ecm_add_qtwayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/protocols/wlr-data-control-unstable-v1.xml
    BASENAME wlr-data-control-unstable-v1
)

ecm_add_qtwayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
    BASENAME keyboard-shortcuts-inhibit-unstable-v1
)

set(SERVER_GENERATED_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-blur-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-blur-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-contrast-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-contrast-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-dpms-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-dpms-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-eglstream-controller-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fake-input-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fake-input-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-inhibit-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-inhibit-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-linux-dmabuf-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-org_kde_kwin_outputdevice-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-org_kde_kwin_outputdevice-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-management-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-management-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-virtual-desktop-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-virtual-desktop-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-window-management-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-window-management-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-constraints-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-constraints-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-gestures-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-gestures-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-qt-surface-extension-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-qt-surface-extension-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-relativepointer-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-relativepointer-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration_palette-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration_palette-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-shadow-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-shadow-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-slide-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-slide-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-input-unstable-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-input-unstable-v2-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-decoration-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-decoration-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-foreign-unstable-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-foreign-unstable-v2-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-v6-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-v6-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-tablet-unstable-v2.h
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-tablet-unstable-v2.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-keyboard-shortcuts-inhibit-unstable-v1.h
)

set_source_files_properties(${SERVER_GENERATED_SRCS} PROPERTIES SKIP_AUTOMOC ON)

add_library(KWaylandServer ${SERVER_LIB_SRCS})
add_library(Plasma::KWaylandServer ALIAS KWaylandServer)
ecm_generate_export_header(KWaylandServer
    BASE_NAME
        KWaylandServer
    EXPORT_FILE_NAME
        KWaylandServer/kwaylandserver_export.h
    GROUP_BASE_NAME KF
    VERSION ${PROJECT_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 5.5 5.28 5.50 5.52 5.69
)
# TODO: add support for EXCLUDE_DEPRECATED_BEFORE_AND_AT to all KWayland libs
# needs fixing of undeprecated API being still implemented using own deprecated API


target_include_directories(KWaylandServer INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>")

target_link_libraries(KWaylandServer
    PUBLIC Qt5::Gui
    PRIVATE
        Wayland::Server
        EGL::EGL
        Qt5::Concurrent
)

set_target_properties(KWaylandServer PROPERTIES VERSION   ${KWAYLANDSERVER_VERSION_STRING}
                                                SOVERSION ${KWAYLANDSERVER_SOVERSION}
)

install(TARGETS KWaylandServer EXPORT KWaylandServerTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

set(SERVER_LIB_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/KWaylandServer/kwaylandserver_export.h
  abstract_data_source.h
  appmenu_interface.h
  blur_interface.h
  buffer_interface.h
  clientconnection.h
  compositor_interface.h
  contrast_interface.h
  datacontroldevice_v1_interface.h
  datacontroldevicemanager_v1_interface.h
  datacontroloffer_v1_interface.h
  datacontrolsource_v1_interface.h
  datadevice_interface.h
  datadevicemanager_interface.h
  dataoffer_interface.h
  datasource_interface.h
  display.h
  dpms_interface.h
  eglstream_controller_interface.h
  fakeinput_interface.h
  filtered_display.h
  global.h
  idle_interface.h
  idleinhibit_interface.h
  keyboard_interface.h
  keyboard_shortcuts_inhibit_v1_interface.h
  keystate_interface.h
  linuxdmabuf_v1_interface.h
  output_interface.h
  outputchangeset.h
  outputconfiguration_interface.h
  outputdevice_interface.h
  outputmanagement_interface.h
  plasmashell_interface.h
  plasmavirtualdesktop_interface.h
  plasmawindowmanagement_interface.h
  pointer_interface.h
  pointerconstraints_interface.h
  pointergestures_interface.h
  region_interface.h
  relativepointer_interface.h
  remote_access_interface.h
  resource.h
  seat_interface.h
  server_decoration_interface.h
  server_decoration_palette_interface.h
  shadow_interface.h
  slide_interface.h
  subcompositor_interface.h
  surface_interface.h
  tablet_interface.h
  textinput_interface.h
  touch_interface.h
  xdgdecoration_interface.h
  xdgforeign_interface.h
  xdgoutput_interface.h
  xdgshell_interface.h
)

install(FILES
  ${SERVER_LIB_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR}/KWaylandServer COMPONENT Devel
)

# make available to ecm_add_qch in parent folder
set(KWaylandServer_APIDOX_SRCS ${SERVER_LIB_HEADERS} PARENT_SCOPE)
set(KWaylandServer_APIDOX_BUILD_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
