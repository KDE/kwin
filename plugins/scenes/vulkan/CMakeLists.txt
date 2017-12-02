set(SCENE_VULKAN_SRCS
    scene.cpp
    swapchain.cpp
    descriptorset.cpp
    window.cpp
    windowpixmap.cpp
    shadow.cpp
    shadowtexturemanager.cpp
    effectframe.cpp
    decorationrenderer.cpp
    quadsplitter.cpp)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(
    SCENE_VULKAN_SRCS HEADER
        logging.h
    IDENTIFIER
        KWIN_VULKAN
    CATEGORY_NAME
        kwin_vulkan_scene
    DEFAULT_SEVERITY
        Debug
)

add_library(KWinSceneVulkan MODULE ${SCENE_VULKAN_SRCS})
target_link_libraries(KWinSceneVulkan
    kwin
    SceneVulkanBackend
    kwinvulkanutils
    Wayland::Server
)

install(
    TARGETS
        KWinSceneVulkan
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.scenes/
)
