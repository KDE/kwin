set(VIRTUAL_SOURCES
    egl_gbm_backend.cpp
    virtual_backend.cpp
    scene_qpainter_virtual_backend.cpp
    screens_virtual.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/platformsupport/scenes/opengl)
include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(VIRTUAL_SOURCES HEADER logging.h IDENTIFIER KWIN_VIRTUAL CATEGORY_NAME kwin_platform_virtual DEFAULT_SEVERITY Critical)

add_library(KWinWaylandVirtualBackend MODULE ${VIRTUAL_SOURCES})
target_link_libraries(KWinWaylandVirtualBackend kwin SceneQPainterBackend SceneOpenGLBackend)

if(HAVE_GBM)
    target_link_libraries(KWinWaylandVirtualBackend gbm::gbm)
endif()

install(
    TARGETS
        KWinWaylandVirtualBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
