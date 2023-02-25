if (XCB_ICCCM_FOUND)
    set(normalhintsbasesizetest_SRCS normalhintsbasesizetest.cpp)
    add_executable(normalhintsbasesizetest ${normalhintsbasesizetest_SRCS})
    target_link_libraries(normalhintsbasesizetest XCB::XCB XCB::ICCCM KF6::WindowSystem)
endif()

# next target
set(screenedgeshowtest_SRCS screenedgeshowtest.cpp)
add_executable(screenedgeshowtest ${screenedgeshowtest_SRCS})
target_link_libraries(screenedgeshowtest Qt::GuiPrivate Qt::Widgets KF6::ConfigCore KF6::WindowSystem KF6::WaylandClient ${XCB_XCB_LIBRARY})
target_include_directories(screenedgeshowtest PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(x11shadowreader x11shadowreader.cpp)
target_link_libraries(x11shadowreader XCB::XCB Qt::GuiPrivate Qt::Widgets KF6::ConfigCore KF6::WindowSystem)
target_include_directories(x11shadowreader PRIVATE ${CMAKE_SOURCE_DIR}/src)

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
