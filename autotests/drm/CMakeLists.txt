set(mockDRM_SRCS
    mock_drm.cpp
    ../../src/backends/drm/drm_abstract_output.cpp
    ../../src/backends/drm/drm_backend.cpp
    ../../src/backends/drm/drm_blob.cpp
    ../../src/backends/drm/drm_buffer.cpp
    ../../src/backends/drm/drm_colorop.cpp
    ../../src/backends/drm/drm_commit.cpp
    ../../src/backends/drm/drm_commit_thread.cpp
    ../../src/backends/drm/drm_connector.cpp
    ../../src/backends/drm/drm_crtc.cpp
    ../../src/backends/drm/drm_egl_backend.cpp
    ../../src/backends/drm/drm_egl_layer.cpp
    ../../src/backends/drm/drm_egl_layer_surface.cpp
    ../../src/backends/drm/drm_gpu.cpp
    ../../src/backends/drm/drm_layer.cpp
    ../../src/backends/drm/drm_logging.cpp
    ../../src/backends/drm/drm_object.cpp
    ../../src/backends/drm/drm_output.cpp
    ../../src/backends/drm/drm_pipeline.cpp
    ../../src/backends/drm/drm_pipeline_legacy.cpp
    ../../src/backends/drm/drm_plane.cpp
    ../../src/backends/drm/drm_property.cpp
    ../../src/backends/drm/drm_qpainter_backend.cpp
    ../../src/backends/drm/drm_qpainter_layer.cpp
    ../../src/backends/drm/drm_virtual_egl_layer.cpp
    ../../src/backends/drm/drm_virtual_output.cpp
)

include_directories(${Libdrm_INCLUDE_DIRS})

add_library(LibDrmTest STATIC ${mockDRM_SRCS})
target_link_libraries(LibDrmTest
    Qt::Gui
    Qt::Widgets
    KF6::ConfigCore
    KF6::WindowSystem
    KF6::CoreAddons
    KF6::I18n
    PkgConfig::Libxcvt
    gbm::gbm
    Libdrm::Libdrm
    kwin
)
target_include_directories(LibDrmTest
    PUBLIC
        ../../src
        ../../src/backends/drm/
)

########################################################
# Tests
########################################################
add_executable(testDrm drmTest.cpp)
target_link_libraries(testDrm LibDrmTest Qt::Test)
add_test(NAME kwin-testDrm COMMAND testDrm)
ecm_mark_as_test(testDrm)
