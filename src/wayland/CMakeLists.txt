set(SERVER_LIB_SRCS
    appmenu_interface.cpp
    buffer_interface.cpp
    clientconnection.cpp
    compositor_interface.cpp
    datadevice_interface.cpp
    datadevicemanager_interface.cpp
    dataoffer_interface.cpp
    datasource_interface.cpp
    display.cpp
    dpms_interface.cpp
    filtered_display.cpp
    global.cpp
    idle_interface.cpp
    idleinhibit_interface.cpp
    idleinhibit_interface_v1.cpp
    fakeinput_interface.cpp
    keyboard_interface.cpp
    keystate_interface.cpp
    remote_access_interface.cpp
    outputconfiguration_interface.cpp
    outputchangeset.cpp
    outputmanagement_interface.cpp
    outputdevice_interface.cpp
    output_interface.cpp
    pointer_interface.cpp
    plasmashell_interface.cpp
    plasmavirtualdesktop_interface.cpp
    plasmawindowmanagement_interface.cpp
    pointerconstraints_interface.cpp
    pointerconstraints_interface_v1.cpp
    pointergestures_interface.cpp
    pointergestures_interface_v1.cpp
    qtsurfaceextension_interface.cpp
    region_interface.cpp
    relativepointer_interface.cpp
    relativepointer_interface_v1.cpp
    resource.cpp
    seat_interface.cpp
    slide_interface.cpp
    shadow_interface.cpp
    blur_interface.cpp
    contrast_interface.cpp
    server_decoration_interface.cpp
    server_decoration_palette_interface.cpp
    shell_interface.cpp
    surface_interface.cpp
    subcompositor_interface.cpp
    touch_interface.cpp
    textinput_interface.cpp
    textinput_interface_v0.cpp
    textinput_interface_v2.cpp
    xdgdecoration_interface.cpp
    xdgshell_interface.cpp
    xdgshell_v5_interface.cpp
    xdgforeign_v2_interface.cpp
    xdgforeign_interface.cpp
    xdgshell_v5_interface.cpp
    xdgshell_v6_interface.cpp
    xdgshell_stable_interface.cpp
    xdgoutput_interface.cpp
    eglstream_controller_interface.cpp
    ../compat/wayland-xdg-shell-v5-protocol.c
)

ecm_qt_declare_logging_category(SERVER_LIB_SRCS HEADER logging.h IDENTIFIER KWAYLAND_SERVER CATEGORY_NAME kwayland-server DEFAULT_SEVERITY Critical)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/output-management.xml
    BASENAME output-management
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/outputdevice.xml
    BASENAME org_kde_kwin_outputdevice
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/plasma-shell.xml
    BASENAME plasma-shell
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/plasma-virtual-desktop.xml
    BASENAME plasma-virtual-desktop
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/plasma-window-management.xml
    BASENAME plasma-window-management
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/surface-extension.xml
    BASENAME qt-surface-extension
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/idle.xml
    BASENAME idle
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/fake-input.xml
    BASENAME fake-input
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/shadow.xml
    BASENAME shadow
)
ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/dpms.xml
    BASENAME dpms
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/blur.xml
    BASENAME blur
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/contrast.xml
    BASENAME contrast
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    BASENAME relativepointer-unstable-v1
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/slide.xml
    BASENAME slide
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/server-decoration.xml
    BASENAME server_decoration
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/text-input.xml
    BASENAME text
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/text-input-unstable-v2.xml
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
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/appmenu.xml
    BASENAME appmenu
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/server-decoration-palette.xml
    BASENAME server_decoration_palette
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/remote-access.xml
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
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/wayland-eglstream-controller.xml
    BASENAME eglstream-controller
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWayland_SOURCE_DIR}/src/client/protocols/keystate.xml
    BASENAME keystate
)

set(SERVER_GENERATED_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-management-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-management-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-org_kde_kwin_outputdevice-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-org_kde_kwin_outputdevice-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-virtual-desktop-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-virtual-desktop-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-window-management-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-window-management-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-qt-surface-extension-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-qt-surface-extension-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fake-input-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-fake-input-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-shadow-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-shadow-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-dpms-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-dpms-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-blur-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-blur-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-contrast-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-contrast-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-relativepointer-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-relativepointer-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-slide-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-slide-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration_palette-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-server_decoration_palette-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-input-unstable-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-text-input-unstable-v2-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-v6-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-v6-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-shell-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-gestures-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-gestures-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-constraints-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-pointer-constraints-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-foreign-unstable-v2-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-foreign-unstable-v2-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-inhibit-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-idle-inhibit-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-unstable-v1-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-output-unstable-v1-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-decoration-server-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-xdg-decoration-client-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-eglstream-controller-server-protocol.h
)

set_source_files_properties(${SERVER_GENERATED_SRCS} PROPERTIES SKIP_AUTOMOC ON)

add_library(KF5WaylandServer ${SERVER_LIB_SRCS})
generate_export_header(KF5WaylandServer
    BASE_NAME
        KWaylandServer
    EXPORT_FILE_NAME
        KWayland/Server/kwaylandserver_export.h
)


add_library(KF5::WaylandServer ALIAS KF5WaylandServer)

target_include_directories(KF5WaylandServer INTERFACE "$<INSTALL_INTERFACE:${KF5_INCLUDE_INSTALL_DIR}/KWayland/Server>")

target_link_libraries(KF5WaylandServer
    PUBLIC Qt5::Gui
    PRIVATE
        Wayland::Server
        EGL::EGL
        Qt5::Concurrent
)

set_target_properties(KF5WaylandServer PROPERTIES VERSION   ${KWAYLAND_VERSION_STRING}
                                                 SOVERSION ${KWAYLAND_SOVERSION}
                                                 EXPORT_NAME WaylandServer
)

install(TARGETS KF5WaylandServer EXPORT KF5WaylandTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

set(SERVER_LIB_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/KWayland/Server/kwaylandserver_export.h
  appmenu_interface.h
  blur_interface.h
  contrast_interface.h
  buffer_interface.h
  clientconnection.h
  compositor_interface.h
  datadevice_interface.h
  datadevicemanager_interface.h
  dataoffer_interface.h
  datasource_interface.h
  display.h
  dpms_interface.h
  eglstream_controller_interface.h
  filtered_display.h
  fakeinput_interface.h
  global.h
  idle_interface.h
  idleinhibit_interface.h
  keyboard_interface.h
  keystate_interface.h
  remote_access_interface.h
  outputdevice_interface.h
  outputchangeset.h
  outputconfiguration_interface.h
  outputmanagement_interface.h
  output_interface.h
  pointer_interface.h
  pointerconstraints_interface.h
  pointergestures_interface.h
  plasmashell_interface.h
  plasmavirtualdesktop_interface.h
  plasmawindowmanagement_interface.h
  qtsurfaceextension_interface.h
  region_interface.h
  relativepointer_interface.h
  resource.h
  seat_interface.h
  server_decoration_interface.h
  server_decoration_palette_interface.h
  shadow_interface.h
  shell_interface.h
  slide_interface.h
  subcompositor_interface.h
  surface_interface.h
  textinput_interface.h
  touch_interface.h
  xdgdecoration_interface.h
  xdgshell_interface.h
  xdgforeign_interface.h
  xdgoutput_interface.h
)

install(FILES
  ${SERVER_LIB_HEADERS}
  DESTINATION ${KF5_INCLUDE_INSTALL_DIR}/KWayland/Server COMPONENT Devel
)

# make available to ecm_add_qch in parent folder
set(KWaylandServer_APIDOX_SRCS ${SERVER_LIB_HEADERS} PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KWaylandServer LIB_NAME KF5WaylandServer DEPS "core" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5})
install(FILES ${PRI_FILENAME}
        DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
