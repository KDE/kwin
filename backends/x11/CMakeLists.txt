set(X11BACKEND_SOURCES
    logging.cpp
    scene_qpainter_x11_backend.cpp
    screens_x11windowed.cpp
    x11windowed_backend.cpp
)

add_library(KWinWaylandX11Backend MODULE ${X11BACKEND_SOURCES})
target_link_libraries(KWinWaylandX11Backend kwin)
if(X11_XCB_FOUND)
    target_link_libraries(KWinWaylandX11Backend X11::XCB)
endif()

install(
    TARGETS
        KWinWaylandX11Backend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
