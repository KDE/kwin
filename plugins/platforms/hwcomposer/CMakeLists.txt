set(HWCOMPOSER_SOURCES
    egl_hwcomposer_backend.cpp
    hwcomposer_backend.cpp
    logging.cpp
    screens_hwcomposer.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/platformsupport/scenes/opengl)
add_library(KWinWaylandHwcomposerBackend MODULE ${HWCOMPOSER_SOURCES})
target_link_libraries(KWinWaylandHwcomposerBackend
    kwin
    libhybris::libhardware
    libhybris::hwcomposer
    libhybris::hybriseglplatform
    SceneOpenGLBackend
)

install(
    TARGETS
        KWinWaylandHwcomposerBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
