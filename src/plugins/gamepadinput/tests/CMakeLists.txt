add_executable(gamepadinspect gamepadinspect.cpp gaminginput_v2.cpp)

qt6_generate_wayland_protocol_client_sources(gamepadinspect
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../gaming-input-unstable-v2.xml
)
target_link_libraries(gamepadinspect Qt::Gui Qt::WaylandClient)
install(TARGETS gamepadinspect DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
