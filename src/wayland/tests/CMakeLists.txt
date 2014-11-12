set(testServer_SRCS
    waylandservertest.cpp
    )
add_executable(testServer ${testServer_SRCS})
target_link_libraries(testServer KF5::WaylandServer)
ecm_mark_as_test(testServer)

find_package(Qt5Widgets ${QT_MIN_VERSION} CONFIG QUIET)
if (Qt5Widgets_FOUND)
    set(testRenderingServer_SRCS
        renderingservertest.cpp
        )
    add_executable(testRenderingServer ${testRenderingServer_SRCS})
    target_link_libraries(testRenderingServer KF5::WaylandServer Qt5::Widgets)
    ecm_mark_as_test(testRenderingServer)
endif()

add_executable(copyClient copyclient.cpp)
target_link_libraries(copyClient KF5::WaylandClient)
ecm_mark_as_test(copyClient)

add_executable(pasteClient pasteclient.cpp)
target_link_libraries(pasteClient KF5::WaylandClient)
ecm_mark_as_test(pasteClient)
