set(SCENE_VULKAN_BACKEND_SRCS backend.cpp)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(SCENE_VULKAN_BACKEND_SRCS
    HEADER
        logging.h
    IDENTIFIER
        KWIN_VULKAN
    CATEGORY_NAME
        kwin_scene_vulkan
    DEFAULT_SEVERITY
        Critical
)

add_library(SceneVulkanBackend STATIC ${SCENE_VULKAN_BACKEND_SRCS})
target_link_libraries(SceneVulkanBackend Qt5::Core)
