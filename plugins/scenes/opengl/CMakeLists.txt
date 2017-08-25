set(SCENE_OPENGL_SRCS scene_opengl.cpp)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(
    SCENE_OPENGL_SRCS HEADER
        logging.h
    IDENTIFIER
        KWIN_OPENGL
    CATEGORY_NAME
        kwin_scene_opengl
    DEFAULT_SEVERITY
        Critical
)

add_library(KWinSceneOpenGL MODULE scene_opengl.cpp)
set_target_properties(KWinSceneOpenGL PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.scenes/")
target_link_libraries(KWinSceneOpenGL
    kwin
    SceneOpenGLBackend
)

install(
    TARGETS
        KWinSceneOpenGL
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.scenes/
)
