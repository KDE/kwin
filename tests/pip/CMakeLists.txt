add_executable(piptest)

target_sources(piptest PRIVATE
    main.cpp
    pipshellsurface.cpp
    pip.cpp
    window.cpp
)

qt6_generate_wayland_protocol_client_sources(piptest
    PRIVATE_CODE
    FILES
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/xx-pip-v1.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
        ${Wayland_DATADIR}/wayland.xml
)

target_link_libraries(piptest PRIVATE
    Qt::Gui
    Qt::WaylandClientPrivate
    Qt::Widgets
)
