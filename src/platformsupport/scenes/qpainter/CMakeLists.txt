set(SCENE_QPAINTER_BACKEND_SRCS qpainterbackend.cpp)

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
target_link_libraries(SceneQPainterBackend Qt::Core)
target_include_directories(SceneQPainterBackend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
