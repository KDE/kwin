set(mockDRM_SRCS
    mock_drm.cpp mock_drm.h
    ../../src/backends/drm/drm_abstract_output.cpp ../../src/backends/drm/drm_abstract_output.h
    ../../src/backends/drm/drm_backend.cpp ../../src/backends/drm/drm_backend.h
    ../../src/backends/drm/drm_buffer.cpp ../../src/backends/drm/drm_buffer.h
    ../../src/backends/drm/drm_buffer_gbm.cpp ../../src/backends/drm/drm_buffer_gbm.h
    ../../src/backends/drm/drm_dmabuf_feedback.cpp ../../src/backends/drm/drm_dmabuf_feedback.h
    ../../src/backends/drm/drm_dumb_buffer.cpp ../../src/backends/drm/drm_dumb_buffer.h
    ../../src/backends/drm/drm_dumb_swapchain.cpp ../../src/backends/drm/drm_dumb_swapchain.h
    ../../src/backends/drm/drm_egl_backend.cpp ../../src/backends/drm/drm_egl_backend.h
    ../../src/backends/drm/drm_egl_cursor_layer.cpp ../../src/backends/drm/drm_egl_cursor_layer.h
    ../../src/backends/drm/drm_egl_layer.cpp ../../src/backends/drm/drm_egl_layer.h
    ../../src/backends/drm/drm_egl_layer_surface.cpp ../../src/backends/drm/drm_egl_layer_surface.h
    ../../src/backends/drm/drm_gbm_surface.cpp ../../src/backends/drm/drm_gbm_surface.h
    ../../src/backends/drm/drm_gpu.cpp ../../src/backends/drm/drm_gpu.h
    ../../src/backends/drm/drm_layer.cpp ../../src/backends/drm/drm_layer.h
    ../../src/backends/drm/drm_logging.cpp ../../src/backends/drm/drm_logging.h
    ../../src/backends/drm/drm_connector.cpp ../../src/backends/drm/drm_connector.h
    ../../src/backends/drm/drm_object.cpp ../../src/backends/drm/drm_object.h
    ../../src/backends/drm/drm_crtc.cpp ../../src/backends/drm/drm_crtc.h
    ../../src/backends/drm/drm_plane.cpp ../../src/backends/drm/drm_plane.h
    ../../src/backends/drm/drm_output.cpp ../../src/backends/drm/drm_output.h
    ../../src/backends/drm/drm_pipeline.cpp ../../src/backends/drm/drm_pipeline.h
    ../../src/backends/drm/drm_pipeline_legacy.cpp ../../src/backends/drm/drm_pipeline_legacy.h
    ../../src/backends/drm/drm_property.cpp ../../src/backends/drm/drm_property.h
    ../../src/backends/drm/drm_qpainter_backend.cpp ../../src/backends/drm/drm_qpainter_backend.h
    ../../src/backends/drm/drm_qpainter_layer.cpp ../../src/backends/drm/drm_qpainter_layer.h
    ../../src/backends/drm/drm_shadow_buffer.cpp ../../src/backends/drm/drm_shadow_buffer.h
    ../../src/backends/drm/drm_virtual_egl_layer.cpp ../../src/backends/drm/drm_virtual_egl_layer.h
    ../../src/backends/drm/drm_virtual_output.cpp ../../src/backends/drm/drm_virtual_output.h
)

include_directories(${Libdrm_INCLUDE_DIRS})

add_library(LibDrmTest STATIC ${mockDRM_SRCS})
target_link_libraries(LibDrmTest
    Qt::Gui
    Qt::Widgets
    KF5::ConfigCore
    KF5::WindowSystem
    KF5::CoreAddons
    KF5::I18n
    XCB::XCB
    PkgConfig::Libxcvt
    gbm::gbm
    kwin
)
target_include_directories(LibDrmTest
    PUBLIC
        ../../src
        ../../src/platformsupport/scenes/opengl
        ../../src/platformsupport/scenes/qpainter
        ../../src/backends/drm/
)

########################################################
# Tests
########################################################
add_executable(testDrm drmTest.cpp drmTest.h)
target_link_libraries(testDrm LibDrmTest Qt::Test)
add_test(NAME kwin-testDrm COMMAND testDrm)
ecm_mark_as_test(testDrm)
