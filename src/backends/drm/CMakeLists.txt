target_sources(kwin PRIVATE
    drm_abstract_output.cpp
    drm_backend.cpp
    drm_buffer.cpp
    drm_buffer_gbm.cpp
    drm_connector.cpp
    drm_crtc.cpp
    drm_dmabuf_feedback.cpp
    drm_dumb_buffer.cpp
    drm_dumb_swapchain.cpp
    drm_egl_backend.cpp
    drm_egl_cursor_layer.cpp
    drm_egl_layer.cpp
    drm_egl_layer_surface.cpp
    drm_gbm_surface.cpp
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
    drm_shadow_buffer.cpp
    drm_virtual_egl_layer.cpp
    drm_virtual_output.cpp
)

target_link_libraries(kwin Libdrm::Libdrm gbm::gbm PkgConfig::Libxcvt)
