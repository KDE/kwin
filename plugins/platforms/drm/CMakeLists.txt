set(DRM_SOURCES
    drm_backend.cpp
    drm_object.cpp
    drm_object_connector.cpp
    drm_object_crtc.cpp
    drm_object_plane.cpp
    drm_output.cpp
    drm_buffer.cpp
    drm_inputeventfilter.cpp
    edid.cpp
    logging.cpp
    scene_qpainter_drm_backend.cpp
    screens_drm.cpp
)

if (HAVE_GBM)
    set(DRM_SOURCES ${DRM_SOURCES}
        egl_gbm_backend.cpp
        drm_buffer_gbm.cpp
        gbm_surface.cpp
        gbm_dmabuf.cpp
    )
endif()

if (HAVE_EGL_STREAMS)
    set(DRM_SOURCES ${DRM_SOURCES}
        egl_stream_backend.cpp
    )
endif()

include_directories(${CMAKE_SOURCE_DIR}/platformsupport/scenes/opengl)

add_library(KWinWaylandDrmBackend MODULE ${DRM_SOURCES})
set_target_properties(KWinWaylandDrmBackend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.waylandbackends/")
target_link_libraries(KWinWaylandDrmBackend kwin Libdrm::Libdrm SceneQPainterBackend SceneOpenGLBackend)

if (HAVE_GBM)
    target_link_libraries(KWinWaylandDrmBackend gbm::gbm)
endif()

install(
    TARGETS
        KWinWaylandDrmBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
