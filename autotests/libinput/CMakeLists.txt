include_directories(${Libinput_INCLUDE_DIRS})

add_definitions(-DKWIN_BUILD_TESTING)
add_library(LibInputTestObjects STATIC ../../src/backends/libinput/device.cpp ../../src/backends/libinput/events.cpp ../../src/core/inputdevice.cpp ../../src/mousebuttons.cpp mock_libinput.cpp)
target_link_libraries(LibInputTestObjects Qt::Test Qt::Widgets Qt::DBus Qt::Gui KF6::ConfigCore)
target_include_directories(LibInputTestObjects PUBLIC ${CMAKE_SOURCE_DIR}/src)

########################################################
# Test Devices
########################################################
add_executable(testLibinputDevice device_test.cpp)
target_link_libraries(testLibinputDevice Qt::Test Qt::DBus Qt::Gui KF6::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputDevice COMMAND testLibinputDevice)
ecm_mark_as_test(testLibinputDevice)

########################################################
# Test Key Event
########################################################
add_executable(testLibinputKeyEvent key_event_test.cpp)
target_link_libraries(testLibinputKeyEvent Qt::Test Qt::DBus Qt::Widgets KF6::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputKeyEvent COMMAND testLibinputKeyEvent)
ecm_mark_as_test(testLibinputKeyEvent)

########################################################
# Test Pointer Event
########################################################
add_executable(testLibinputPointerEvent pointer_event_test.cpp)
target_link_libraries(testLibinputPointerEvent Qt::Test Qt::DBus Qt::Widgets KF6::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputPointerEvent COMMAND testLibinputPointerEvent)
ecm_mark_as_test(testLibinputPointerEvent)

########################################################
# Test Touch Event
########################################################
add_executable(testLibinputTouchEvent touch_event_test.cpp)
target_link_libraries(testLibinputTouchEvent Qt::Test Qt::DBus Qt::Widgets KF6::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputTouchEvent COMMAND testLibinputTouchEvent)
ecm_mark_as_test(testLibinputTouchEvent)

########################################################
# Test Gesture Event
########################################################
add_executable(testLibinputGestureEvent gesture_event_test.cpp)
target_link_libraries(testLibinputGestureEvent Qt::Test Qt::DBus Qt::Widgets KF6::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputGestureEvent COMMAND testLibinputGestureEvent)
ecm_mark_as_test(testLibinputGestureEvent)
