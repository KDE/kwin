add_library(KWinWaylandVirtualBackend MODULE)
target_sources(KWinWaylandVirtualBackend PRIVATE
    egl_gbm_backend.cpp
    scene_qpainter_virtual_backend.cpp
    virtual_backend.cpp
    virtual_output.cpp
)

ecm_qt_declare_logging_category(KWinWaylandVirtualBackend
    HEADER logging.h
    IDENTIFIER KWIN_VIRTUAL
    CATEGORY_NAME kwin_platform_virtual
    DEFAULT_SEVERITY Critical
)

set_target_properties(KWinWaylandVirtualBackend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.waylandbackends/")
target_link_libraries(KWinWaylandVirtualBackend kwin)

install(
    TARGETS
        KWinWaylandVirtualBackend
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/org.kde.kwin.waylandbackends/
)
