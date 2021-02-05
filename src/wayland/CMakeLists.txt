set(SERVER_LIB_SRCS
    abstract_data_source.cpp
    abstract_drop_handler.cpp
    appmenu_interface.cpp
    blur_interface.cpp
    clientbuffer.cpp
    clientbufferintegration.cpp
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
    drmclientbuffer.cpp
    drmleasedevice_v1_interface.cpp
    eglstream_controller_interface.cpp
    fakeinput_interface.cpp
    filtered_display.cpp
    idle_interface.cpp
    idleinhibit_v1_interface.cpp
    inputmethod_v1_interface.cpp
    keyboard_interface.cpp
    keyboard_shortcuts_inhibit_v1_interface.cpp
    keystate_interface.cpp
    layershell_v1_interface.cpp
    linuxdmabufv1clientbuffer.cpp
    output_interface.cpp
    outputdevice_v2_interface.cpp
    outputconfiguration_v2_interface.cpp
    outputmanagement_v2_interface.cpp
    outputchangeset_v2.cpp
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
    textinput.cpp
    textinput_v2_interface.cpp
    textinput_v3_interface.cpp
    touch_interface.cpp
    viewporter_interface.cpp
    xdgactivation_v1_interface.cpp
    xdgdecoration_v1_interface.cpp
    xdgforeign_v2_interface.cpp
    xdgoutput_v1_interface.cpp
    xdgshell_interface.cpp
)

ecm_qt_declare_logging_category(SERVER_LIB_SRCS
    HEADER logging.h
    IDENTIFIER KWAYLAND_SERVER
    CATEGORY_NAME kwayland-server
    DEFAULT_SEVERITY Critical
    DESCRIPTION "KWayland Server Library"
    EXPORT KWAYLAND
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${Wayland_DATADIR}/wayland.xml
    BASENAME wayland
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-device-v2.xml
    BASENAME kde-output-device-v2
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-management-v2.xml
    BASENAME kde-output-management-v2
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-shell.xml
    BASENAME plasma-shell
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-virtual-desktop.xml
    BASENAME org-kde-plasma-virtual-desktop
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-window-management.xml
    BASENAME plasma-window-management
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/surface-extension.xml
    BASENAME qt-surface-extension
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/idle.xml
    BASENAME idle
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml
    BASENAME fake-input
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/shadow.xml
    BASENAME shadow
)
ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/dpms.xml
    BASENAME dpms
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/blur.xml
    BASENAME blur
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/contrast.xml
    BASENAME contrast
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    BASENAME relative-pointer-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/slide.xml
    BASENAME slide
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/server-decoration.xml
    BASENAME server-decoration
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v1.xml
    BASENAME text-input-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/text-input-unstable-v2.xml
    BASENAME text-input-unstable-v2
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
    BASENAME text-input-unstable-v3
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
    BASENAME pointer-gestures-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
    BASENAME pointer-constraints-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml
    BASENAME xdg-foreign-unstable-v2
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
    BASENAME idle-inhibit-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/appmenu.xml
    BASENAME appmenu
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/server-decoration-palette.xml
    BASENAME server-decoration-palette
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-output/xdg-output-unstable-v1.xml
    BASENAME xdg-output-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
    BASENAME xdg-shell
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
    BASENAME xdg-decoration-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/wayland-eglstream-controller.xml
    BASENAME wl-eglstream-controller
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/keystate.xml
    BASENAME keystate
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
    BASENAME linux-dmabuf-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/tablet/tablet-unstable-v2.xml
    BASENAME tablet-unstable-v2
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/protocols/wlr-data-control-unstable-v1.xml
    BASENAME wlr-data-control-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PROJECT_SOURCE_DIR}/src/protocols/wlr-layer-shell-unstable-v1.xml
    BASENAME wlr-layer-shell-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
    BASENAME keyboard-shortcuts-inhibit-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/stable/viewporter/viewporter.xml
    BASENAME viewporter
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/primary-selection/primary-selection-unstable-v1.xml
    BASENAME wp-primary-selection-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/screencast.xml
    BASENAME zkde-screencast-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
    BASENAME input-method-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/xdg-activation/xdg-activation-v1.xml
    BASENAME xdg-activation-v1
)

ecm_add_qtwayland_server_protocol_kde(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/staging/drm-lease/drm-lease-v1.xml
    BASENAME drm-lease-v1
)

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
    PUBLIC
        Qt::Gui
        Wayland::Server
    PRIVATE
        EGL::EGL
        Qt::Concurrent
)

target_compile_definitions(KWaylandServer PRIVATE
    MESA_EGL_NO_X11_HEADERS
    EGL_NO_X11
    EGL_NO_PLATFORM_SPECIFIC_TYPES
)

set_target_properties(KWaylandServer PROPERTIES VERSION   ${KWAYLANDSERVER_VERSION}
                                                SOVERSION ${KWAYLANDSERVER_SOVERSION}
)

install(TARGETS KWaylandServer EXPORT KWaylandServerTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

set(SERVER_LIB_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/KWaylandServer/kwaylandserver_export.h
  abstract_data_source.h
  abstract_drop_handler.h
  appmenu_interface.h
  blur_interface.h
  clientbuffer.h
  clientbufferintegration.h
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
  drmclientbuffer.h
  drmleasedevice_v1_interface.h
  eglstream_controller_interface.h
  fakeinput_interface.h
  filtered_display.h
  idle_interface.h
  idleinhibit_v1_interface.h
  inputmethod_v1_interface.h
  keyboard_interface.h
  keyboard_shortcuts_inhibit_v1_interface.h
  keystate_interface.h
  layershell_v1_interface.h
  linuxdmabufv1clientbuffer.h
  output_interface.h
  outputchangeset_v2.h
  outputconfiguration_v2_interface.h
  outputdevice_v2_interface.h
  outputmanagement_v2_interface.h
  plasmashell_interface.h
  plasmavirtualdesktop_interface.h
  plasmawindowmanagement_interface.h
  pointer_interface.h
  pointerconstraints_v1_interface.h
  pointergestures_v1_interface.h
  primaryselectiondevice_v1_interface.h
  primaryselectiondevicemanager_v1_interface.h
  primaryselectionoffer_v1_interface.h
  primaryselectionsource_v1_interface.h
  relativepointer_v1_interface.h
  screencast_v1_interface.h
  seat_interface.h
  server_decoration_interface.h
  server_decoration_palette_interface.h
  shadow_interface.h
  shmclientbuffer.h
  slide_interface.h
  subcompositor_interface.h
  surface_interface.h
  tablet_v2_interface.h
  textinput.h
  textinput_v2_interface.h
  textinput_v3_interface.h
  touch_interface.h
  utils.h
  viewporter_interface.h
  xdgactivation_v1_interface.h
  xdgdecoration_v1_interface.h
  xdgforeign_v2_interface.h
  xdgoutput_v1_interface.h
  xdgshell_interface.h
)

install(FILES
  ${SERVER_LIB_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR}/KWaylandServer COMPONENT Devel
)

# make available to ecm_add_qch in parent folder
set(KWaylandServer_APIDOX_SRCS ${SERVER_LIB_HEADERS} PARENT_SCOPE)
set(KWaylandServer_APIDOX_BUILD_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
