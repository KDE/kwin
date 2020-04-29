set(SCENE_OPENGL_BACKEND_SRCS
    abstract_egl_backend.cpp
    backend.cpp
    egl_dmabuf.cpp
    swap_profiler.cpp
    texture.cpp
)

include_directories(${CMAKE_SOURCE_DIR})

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(SCENE_OPENGL_BACKEND_SRCS
    HEADER
        logging.h
    IDENTIFIER
        KWIN_OPENGL
    CATEGORY_NAME
        kwin_scene_opengl
    DEFAULT_SEVERITY
        Critical
)

add_library(SceneOpenGLBackend STATIC ${SCENE_OPENGL_BACKEND_SRCS})
target_link_libraries(SceneOpenGLBackend Qt5::Core Qt5::Widgets KF5::CoreAddons KF5::ConfigCore KF5::WindowSystem Plasma::KWaylandServer)
