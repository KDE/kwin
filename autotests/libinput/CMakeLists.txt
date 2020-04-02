include_directories(${Libinput_INCLUDE_DIRS})
include_directories(${UDEV_INCLUDE_DIR})

add_library(LibInputTestObjects STATIC ../../libinput/device.cpp ../../libinput/events.cpp mock_libinput.cpp)
target_link_libraries(LibInputTestObjects Qt5::Test Qt5::Widgets Qt5::DBus Qt5::Gui KF5::ConfigCore)

########################################################
# Test Devices
########################################################
add_executable(testLibinputDevice device_test.cpp)
target_link_libraries(testLibinputDevice Qt5::Test Qt5::DBus Qt5::Gui KF5::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputDevice COMMAND testLibinputDevice)
ecm_mark_as_test(testLibinputDevice)

########################################################
# Test Key Event
########################################################
add_executable(testLibinputKeyEvent key_event_test.cpp)
target_link_libraries(testLibinputKeyEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputKeyEvent COMMAND testLibinputKeyEvent)
ecm_mark_as_test(testLibinputKeyEvent)

########################################################
# Test Pointer Event
########################################################
add_executable(testLibinputPointerEvent pointer_event_test.cpp)
target_link_libraries(testLibinputPointerEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputPointerEvent COMMAND testLibinputPointerEvent)
ecm_mark_as_test(testLibinputPointerEvent)

########################################################
# Test Touch Event
########################################################
add_executable(testLibinputTouchEvent touch_event_test.cpp)
target_link_libraries(testLibinputTouchEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputTouchEvent COMMAND testLibinputTouchEvent)
ecm_mark_as_test(testLibinputTouchEvent)

########################################################
# Test Gesture Event
########################################################
add_executable(testLibinputGestureEvent gesture_event_test.cpp)
target_link_libraries(testLibinputGestureEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputGestureEvent COMMAND testLibinputGestureEvent)
ecm_mark_as_test(testLibinputGestureEvent)

########################################################
# Test Switch Event
########################################################
add_executable(testLibinputSwitchEvent switch_event_test.cpp)
target_link_libraries(testLibinputSwitchEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testLibinputSwitchEvent COMMAND testLibinputSwitchEvent)
ecm_mark_as_test(testLibinputSwitchEvent)

########################################################
# Test Context
########################################################
set(testLibinputContext_SRCS
        ../../libinput/context.cpp
        ../../libinput/libinput_logging.cpp
        ../../logind.cpp
        context_test.cpp
        mock_udev.cpp
)
add_executable(testLibinputContext ${testLibinputContext_SRCS})
target_link_libraries(testLibinputContext
    LibInputTestObjects

    Qt5::DBus
    Qt5::Test
    Qt5::Widgets

    KF5::ConfigCore
    KF5::WindowSystem
)
add_test(NAME kwin-testLibinputContext COMMAND testLibinputContext)
ecm_mark_as_test(testLibinputContext)

########################################################
# Test Input Events
########################################################
add_executable(testInputEvents input_event_test.cpp ../../input_event.cpp)
target_link_libraries(testInputEvents Qt5::Test Qt5::DBus Qt5::Gui Qt5::Widgets KF5::ConfigCore LibInputTestObjects)
add_test(NAME kwin-testInputEvents COMMAND testInputEvents)
ecm_mark_as_test(testInputEvents)
