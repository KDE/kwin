set(SERVER_LIB_SRCS
    buffer_interface.cpp
    clientconnection.cpp
    compositor_interface.cpp
    datadevice_interface.cpp
    datadevicemanager_interface.cpp
    dataoffer_interface.cpp
    datasource_interface.cpp
    display.cpp
    global.cpp
    keyboard_interface.cpp
    logging.cpp
    output_interface.cpp
    pointer_interface.cpp
    plasmashell_interface.cpp
    plasmawindowmanagement_interface.cpp
    qtsurfaceextension_interface.cpp
    region_interface.cpp
    resource.cpp
    seat_interface.cpp
    shell_interface.cpp
    surface_interface.cpp
    subcompositor_interface.cpp
    touch_interface.cpp
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWAYLAND_SOURCE_DIR}/src/client/protocols/plasma-shell.xml
    BASENAME plasma-shell
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWAYLAND_SOURCE_DIR}/src/client/protocols/plasma-window-management.xml
    BASENAME plasma-window-management
)

ecm_add_wayland_server_protocol(SERVER_LIB_SRCS
    PROTOCOL ${KWAYLAND_SOURCE_DIR}/src/client/protocols/surface-extension.xml
    BASENAME qt-surface-extension
)

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
)

if(IS_ABSOLUTE "${KF5_INCLUDE_INSTALL_DIR}")
  target_include_directories(KF5WaylandServer INTERFACE "$<INSTALL_INTERFACE:${KF5_INCLUDE_INSTALL_DIR}>" )
else()
  target_include_directories(KF5WaylandServer INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/${KF5_INCLUDE_INSTALL_DIR}>" )
endif()

set_target_properties(KF5WaylandServer PROPERTIES VERSION   ${KWAYLAND_VERSION_STRING}
                                                 SOVERSION ${KWAYLAND_SOVERSION}
                                                 EXPORT_NAME WaylandServer
)

install(TARGETS KF5WaylandServer EXPORT KF5WaylandTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/KWayland/Server/kwaylandserver_export.h
  buffer_interface.h
  clientconnection.h
  compositor_interface.h
  datadevice_interface.h
  datadevicemanager_interface.h
  dataoffer_interface.h
  datasource_interface.h
  display.h
  global.h
  keyboard_interface.h
  output_interface.h
  pointer_interface.h
  plasmashell_interface.h
  plasmawindowmanagement_interface.h
  qtsurfaceextension_interface.h
  region_interface.h
  resource.h
  seat_interface.h
  shell_interface.h
  subcompositor_interface.h
  surface_interface.h
  touch_interface.h
  DESTINATION ${KF5_INCLUDE_INSTALL_DIR}/KWayland/Server COMPONENT Devel
)
