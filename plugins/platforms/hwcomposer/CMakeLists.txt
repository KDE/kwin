set(HWCOMPOSER_SOURCES
    egl_hwcomposer_backend.cpp
    hwcomposer_backend.cpp
    logging.cpp
    screens_hwcomposer.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/platformsupport/scenes/opengl)
add_library(KWinWaylandHwcomposerBackend MODULE ${HWCOMPOSER_SOURCES})
set_target_properties(KWinWaylandHwcomposerBackend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.waylandbackends/")
target_link_libraries(KWinWaylandHwcomposerBackend
    kwin

    SceneOpenGLBackend

    libhybris::hwcomposer
    libhybris::hybriseglplatform
    libhybris::libhardware
)

install(
    TARGETS
        KWinWaylandHwcomposerBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
