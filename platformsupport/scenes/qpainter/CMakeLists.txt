set(SCENE_QPAINTER_BACKEND_SRCS backend.cpp)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(SCENE_QPAINTER_BACKEND_SRCS
    HEADER
        logging.h
    IDENTIFIER
        KWIN_QPAINTER
    CATEGORY_NAME
        kwin_scene_qpainter
    DEFAULT_SEVERITY
        Critical
)

add_library(SceneQPainterBackend STATIC ${SCENE_QPAINTER_BACKEND_SRCS})
target_link_libraries(SceneQPainterBackend Qt5::Core)
