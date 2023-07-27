include(ECMMarkAsTest)
add_executable(test_gamepadv2 test_gamepadv2.cpp ../tests/gaminginput_v2.cpp)
target_link_libraries(test_gamepadv2 PRIVATE kwin KF6::WaylandClient Qt::WaylandClient Qt::Test)
target_include_directories(test_gamepadv2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../tests
                                                  ${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/../tests)
add_test(NAME kwin-testGamepad COMMAND testGamepad)
ecm_mark_as_test(testGamepad)


qt6_generate_wayland_protocol_client_sources(test_gamepadv2
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../gaming-input-unstable-v2.xml
)
