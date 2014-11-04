set(SERVER_LIB_SRCS
    buffer_interface.cpp
    compositor_interface.cpp
    datadevicemanager_interface.cpp
    datasource_interface.cpp
    display.cpp
    output_interface.cpp
    region_interface.cpp
    seat_interface.cpp
    shell_interface.cpp
    surface_interface.cpp
    subcompositor_interface.cpp
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
    PRIVATE Wayland::Server
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

# install(TARGETS KF5WaylandServer EXPORT KF5WaylandTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})
#
# install(FILES
#   ${CMAKE_CURRENT_BINARY_DIR}/KWayland/Server/kwaylandserver_export.h
#   buffer_interface.h
#   compositor_interface.h
#   datadevicemanager_interface.h
#   datasource_interface.h
#   display.h
#   output_interface.h
#   region_interface.h
#   seat_interface.h
#   shell_interface.h
#   surface_interface.h
#   DESTINATION ${KF5_INCLUDE_INSTALL_DIR}/KWayland/Server COMPONENT Devel
# )
