target_sources(kwin PRIVATE
    drm_abstract_output.cpp
    drm_backend.cpp
    drm_blob.cpp
    drm_buffer.cpp
    drm_colorop.cpp
    drm_commit.cpp
    drm_commit_thread.cpp
    drm_connector.cpp
    drm_crtc.cpp
    drm_egl_backend.cpp
    drm_egl_layer.cpp
    drm_egl_layer_surface.cpp
    drm_gpu.cpp
    drm_layer.cpp
    drm_logging.cpp
    drm_object.cpp
    drm_output.cpp
    drm_pipeline.cpp
    drm_pipeline_legacy.cpp
    drm_plane.cpp
    drm_property.cpp
    drm_qpainter_backend.cpp
    drm_qpainter_layer.cpp
    drm_virtual_egl_layer.cpp
    drm_virtual_output.cpp
)

target_link_libraries(kwin PRIVATE gbm::gbm PkgConfig::Libxcvt)
