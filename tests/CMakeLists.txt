set(normalhintsbasesizetest_SRCS normalhintsbasesizetest.cpp)
add_executable(normalhintsbasesizetest ${normalhintsbasesizetest_SRCS})
target_link_libraries(normalhintsbasesizetest XCB::XCB XCB::ICCCM KF6::WindowSystem)

# next target
set(screenedgeshowtest_SRCS screenedgeshowtest.cpp)
add_executable(screenedgeshowtest ${screenedgeshowtest_SRCS})
target_link_libraries(screenedgeshowtest Qt::GuiPrivate Qt::Widgets KF6::ConfigCore KF6::WindowSystem KF6::WaylandClient ${XCB_XCB_LIBRARY})

add_executable(x11shadowreader x11shadowreader.cpp)
target_link_libraries(x11shadowreader XCB::XCB Qt::GuiPrivate Qt::Widgets KF6::ConfigCore KF6::WindowSystem)

add_executable(pointerconstraints pointerconstraintstest.cpp)
add_definitions(-DDIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pointerconstraints XCB::XCB Qt::Gui Qt::Quick KF6::WaylandClient)

add_executable(pointergestures pointergesturestest.cpp)
add_definitions(-DDIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pointergestures Qt::Gui Qt::Quick KF6::WaylandClient)

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
target_link_libraries(copyClient KF6::WaylandClient)

add_executable(pasteClient pasteclient.cpp)
target_link_libraries(pasteClient Qt::Core KF6::WaylandClient)

add_executable(touchClientTest touchclienttest.cpp)
target_link_libraries(touchClientTest KF6::WaylandClient)

add_executable(panelTest paneltest.cpp)
target_link_libraries(panelTest KF6::WaylandClient)

add_executable(subsurface-test subsurfacetest.cpp)
target_link_libraries(subsurface-test Qt::Core Qt::Gui KF6::WaylandClient)

add_executable(shadowTest shadowtest.cpp)
target_link_libraries(shadowTest KF6::WaylandClient)

if (TARGET Qt::Widgets)
    add_executable(dpmsTest dpmstest.cpp)
    target_link_libraries(dpmsTest KF6::WaylandClient Qt::Widgets)
endif()

add_executable(plasmasurface-test plasmasurfacetest.cpp)
target_link_libraries(plasmasurface-test Qt::Gui KF6::WaylandClient)

add_executable(xdgforeign-test xdgforeigntest.cpp)
target_link_libraries(xdgforeign-test Qt::Gui KF6::WaylandClient)

add_executable(xdg-test xdgtest.cpp)
target_link_libraries(xdg-test Qt::Gui KF6::WaylandClient)
