target_sources(kwin PRIVATE
    drm_abstract_output.cpp drm_abstract_output.h
    drm_backend.cpp drm_backend.h
    drm_buffer.cpp drm_buffer.h
    drm_buffer_gbm.cpp drm_buffer_gbm.h
    drm_connector.cpp drm_connector.h
    drm_crtc.cpp drm_crtc.h
    drm_dmabuf_feedback.cpp drm_dmabuf_feedback.h
    drm_dumb_buffer.cpp drm_dumb_buffer.h
    drm_dumb_swapchain.cpp drm_dumb_swapchain.h
    drm_egl_backend.cpp drm_egl_backend.h
    drm_egl_cursor_layer.cpp drm_egl_cursor_layer.h
    drm_egl_layer.cpp drm_egl_layer.h
    drm_egl_layer_surface.cpp drm_egl_layer_surface.h
    drm_gbm_surface.cpp drm_gbm_surface.h
    drm_gpu.cpp drm_gpu.h
    drm_layer.cpp drm_layer.h
    drm_logging.cpp drm_logging.h
    drm_object.cpp drm_object.h
    drm_output.cpp drm_output.h
    drm_pipeline.cpp drm_pipeline.h
    drm_pipeline_legacy.cpp drm_pipeline_legacy.h
    drm_plane.cpp drm_plane.h
    drm_property.cpp drm_property.h
    drm_qpainter_backend.cpp drm_qpainter_backend.h
    drm_qpainter_layer.cpp drm_qpainter_layer.h
    drm_shadow_buffer.cpp drm_shadow_buffer.h
    drm_virtual_egl_layer.cpp drm_virtual_egl_layer.h
    drm_virtual_output.cpp drm_virtual_output.h
)

target_link_libraries(kwin Libdrm::Libdrm gbm::gbm PkgConfig::Libxcvt)
