if (XCB_ICCCM_FOUND)
    set(normalhintsbasesizetest_SRCS normalhintsbasesizetest.cpp)
    add_executable(normalhintsbasesizetest ${normalhintsbasesizetest_SRCS})
    target_link_libraries(normalhintsbasesizetest XCB::XCB XCB::ICCCM KF5::WindowSystem)
endif()

# next target
set(screenedgeshowtest_SRCS screenedgeshowtest.cpp)
add_executable(screenedgeshowtest ${screenedgeshowtest_SRCS})
target_link_libraries(screenedgeshowtest Qt5::Widgets Qt5::X11Extras KF5::ConfigCore KF5::WindowSystem KF5::WaylandClient ${XCB_XCB_LIBRARY})

set(libinputtest_SRCS
    ${KWin_SOURCE_DIR}/libinput/connection.cpp
    ${KWin_SOURCE_DIR}/libinput/context.cpp
    ${KWin_SOURCE_DIR}/libinput/device.cpp
    ${KWin_SOURCE_DIR}/libinput/events.cpp
    ${KWin_SOURCE_DIR}/libinput/libinput_logging.cpp
    ${KWin_SOURCE_DIR}/logind.cpp
    ${KWin_SOURCE_DIR}/udev.cpp
    libinputtest.cpp
)
add_executable(libinputtest ${libinputtest_SRCS})
add_definitions(-DKWIN_BUILD_TESTING)
target_link_libraries(libinputtest Qt5::Core Qt5::DBus Libinput::Libinput ${UDEV_LIBS} KF5::ConfigCore KF5::GlobalAccel KF5::WindowSystem)

add_executable(x11shadowreader x11shadowreader.cpp)
target_link_libraries(x11shadowreader XCB::XCB Qt5::Widgets Qt5::X11Extras KF5::ConfigCore KF5::WindowSystem)

add_executable(pointerconstraints pointerconstraintstest.cpp)
add_definitions(-DDIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pointerconstraints XCB::XCB Qt5::Gui Qt5::Quick KF5::WaylandClient)

add_executable(pointergestures pointergesturestest.cpp)
add_definitions(-DDIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pointergestures Qt5::Gui Qt5::Quick KF5::WaylandClient)

add_executable(cursorhotspottest cursorhotspottest.cpp)
target_link_libraries(cursorhotspottest Qt5::Widgets)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
