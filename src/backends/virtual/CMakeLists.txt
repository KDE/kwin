set(VIRTUAL_SOURCES
    egl_gbm_backend.cpp
    scene_qpainter_virtual_backend.cpp
    virtual_backend.cpp
    virtual_output.cpp
)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(VIRTUAL_SOURCES HEADER logging.h IDENTIFIER KWIN_VIRTUAL CATEGORY_NAME kwin_platform_virtual DEFAULT_SEVERITY Critical)

add_library(KWinWaylandVirtualBackend MODULE ${VIRTUAL_SOURCES})
set_target_properties(KWinWaylandVirtualBackend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.waylandbackends/")
target_link_libraries(KWinWaylandVirtualBackend kwin)

install(
    TARGETS
        KWinWaylandVirtualBackend
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/org.kde.kwin.waylandbackends/
)
