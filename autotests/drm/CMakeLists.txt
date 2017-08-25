include_directories(${Libdrm_INCLUDE_DIRS})

set(mockDRM_SRCS
    mock_drm.cpp
    ../../plugins/platforms/drm/drm_buffer.cpp
    ../../plugins/platforms/drm/drm_object.cpp
    ../../plugins/platforms/drm/drm_object_connector.cpp
    ../../plugins/platforms/drm/drm_object_plane.cpp
    ../../plugins/platforms/drm/logging.cpp
)

add_library(mockDrm STATIC ${mockDRM_SRCS})
target_link_libraries(mockDrm Qt5::Gui)
ecm_mark_as_test(mockDrm)

function(drmTest)
    set(oneValueArgs NAME)
    set(multiValueArgs SRCS )
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    add_executable(${ARGS_NAME} ${ARGS_SRCS})
    target_link_libraries(${ARGS_NAME} mockDrm Qt5::Test)
    add_test(NAME kwin-drm-${ARGS_NAME} COMMAND ${ARGS_NAME})
    ecm_mark_as_test(${ARGS_NAME})
endfunction()

drmTest(NAME objecttest SRCS objecttest.cpp)
