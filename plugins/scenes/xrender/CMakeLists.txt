set(SCENE_XRENDER_SRCS scene_xrender.cpp)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(
    SCENE_XRENDER_SRCS HEADER
        logging.h
    IDENTIFIER
        KWIN_XRENDER
    CATEGORY_NAME
        kwin_scene_xrender
    DEFAULT_SEVERITY
        Critical
)

add_library(KWinSceneXRender MODULE ${SCENE_XRENDER_SRCS})
target_link_libraries(KWinSceneXRender
    kwin
)

install(
    TARGETS
        KWinSceneXRender
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.scenes/
)
