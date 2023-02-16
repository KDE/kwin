if (Qt6_VERSION VERSION_GREATER_EQUAL "6.9.0")
    add_subdirectory(pip)
endif()

if(KWIN_BUILD_X11)
    set(normalhintsbasesizetest_SRCS normalhintsbasesizetest.cpp)
    add_executable(normalhintsbasesizetest ${normalhintsbasesizetest_SRCS})
    target_link_libraries(normalhintsbasesizetest XCB::XCB XCB::ICCCM KF6::WindowSystem)

    add_executable(x11shadowreader x11shadowreader.cpp)
    target_link_libraries(x11shadowreader XCB::XCB Qt::GuiPrivate Qt::Widgets KF6::ConfigCore KF6::WindowSystem)

    add_executable(pointerconstraints pointerconstraintstest.cpp)
    add_definitions(-DDIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(pointerconstraints XCB::XCB Qt::Gui Qt::Quick Plasma::KWaylandClient)
endif()

add_executable(pointergestures pointergesturestest.cpp)
add_definitions(-DDIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pointergestures Qt::Gui Qt::Quick Plasma::KWaylandClient)

add_executable(cursorhotspottest cursorhotspottest.cpp)
target_link_libraries(cursorhotspottest Qt::Widgets)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(xdgactivationtest-qt6 xdgactivationtest-qt6.cpp)
target_link_libraries(xdgactivationtest-qt6 Qt6::Widgets)

set(testServer_SRCS
    waylandservertest.cpp
    fakeoutput.cpp
    )
add_executable(testServer ${testServer_SRCS})
target_link_libraries(testServer kwin Qt::CorePrivate)

find_package(Qt6Widgets ${QT_MIN_VERSION} CONFIG QUIET)
if (TARGET Qt::Widgets)
    set(testRenderingServer_SRCS
        renderingservertest.cpp
        fakeoutput.cpp
        )
    add_executable(testRenderingServer ${testRenderingServer_SRCS})
    target_link_libraries(testRenderingServer kwin Qt::Core Qt::Widgets)
endif()

add_executable(copyClient copyclient.cpp)
target_link_libraries(copyClient Plasma::KWaylandClient)

add_executable(pasteClient pasteclient.cpp)
target_link_libraries(pasteClient Qt::Core Plasma::KWaylandClient)

add_executable(touchClientTest touchclienttest.cpp)
target_link_libraries(touchClientTest Plasma::KWaylandClient)

add_executable(panelTest paneltest.cpp)
target_link_libraries(panelTest Plasma::KWaylandClient)

add_executable(subsurface-test subsurfacetest.cpp)
target_link_libraries(subsurface-test Qt::Core Qt::Gui Plasma::KWaylandClient)

add_executable(shadowTest shadowtest.cpp)
target_link_libraries(shadowTest Plasma::KWaylandClient)

if (TARGET Qt::Widgets)
    add_executable(dpmsTest dpmstest.cpp)
    target_link_libraries(dpmsTest Plasma::KWaylandClient Qt::Widgets)
endif()

add_executable(plasmasurface-test plasmasurfacetest.cpp)
target_link_libraries(plasmasurface-test Qt::Gui Plasma::KWaylandClient)

add_executable(xdgforeign-test xdgforeigntest.cpp)
target_link_libraries(xdgforeign-test Qt::Gui Plasma::KWaylandClient)

add_executable(xdg-test xdgtest.cpp)
target_link_libraries(xdg-test Qt::Gui Plasma::KWaylandClient)

if (Qt6_VERSION VERSION_GREATER_EQUAL "6.8.0")
    set(private_code_option  "PRIVATE_CODE")
endif()
add_executable(xdgsessiontest xdgsessiontest.cpp)
qt6_generate_wayland_protocol_client_sources(xdgsessiontest
    ${private_code_option}
    NO_INCLUDE_CORE_ONLY
    FILES
     ${PROJECT_SOURCE_DIR}/src/wayland/protocols/xx-session-management-v1.xml
     ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
)
target_link_libraries(xdgsessiontest
    Qt::Core
    Qt::Widgets
    Qt::GuiPrivate
    Qt::WaylandClient
)
