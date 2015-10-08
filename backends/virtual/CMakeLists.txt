set(VIRTUAL_SOURCES
    egl_gbm_backend.cpp
    virtual_backend.cpp
    scene_qpainter_virtual_backend.cpp
    screens_virtual.cpp
)

add_library(KWinWaylandVirtualBackend MODULE ${VIRTUAL_SOURCES})
target_link_libraries(KWinWaylandVirtualBackend kwin)

install(
    TARGETS
        KWinWaylandVirtualBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
