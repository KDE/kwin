set(FBDEV_SOURCES
    fb_backend.cpp
    logging.cpp
    scene_qpainter_fb_backend.cpp
)

add_library(KWinWaylandFbdevBackend MODULE ${FBDEV_SOURCES})
target_link_libraries(KWinWaylandFbdevBackend kwin SceneQPainterBackend)

install(
    TARGETS
        KWinWaylandFbdevBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
