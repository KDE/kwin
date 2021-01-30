set(xrenderbackend_SOURCES xrenderbackend.cpp)

ecm_qt_declare_logging_category(xrenderbackend_SOURCES
    HEADER logging.h
    IDENTIFIER KWIN_XRENDER
    CATEGORY_NAME kwin_scene_xrender
    DEFAULT_SEVERITY Critical
)

add_library(SceneXRenderBackend STATIC ${xrenderbackend_SOURCES})
target_link_libraries(SceneXRenderBackend kwin Qt::Core Qt::Gui)
target_include_directories(SceneXRenderBackend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
