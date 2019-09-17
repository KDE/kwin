include_directories(${Libinput_INCLUDE_DIRS})
include_directories(${UDEV_INCLUDE_DIR})
########################################################
# Test Devices
########################################################
set(testLibinputDevice_SRCS device_test.cpp mock_libinput.cpp ../../libinput/device.cpp )
add_executable(testLibinputDevice ${testLibinputDevice_SRCS})
target_link_libraries(testLibinputDevice Qt5::Test Qt5::DBus Qt5::Gui KF5::ConfigCore)
add_test(NAME kwin-testLibinputDevice COMMAND testLibinputDevice)
ecm_mark_as_test(testLibinputDevice)

########################################################
# Test Key Event
########################################################
set(testLibinputKeyEvent_SRCS
    ../../libinput/device.cpp
    ../../libinput/events.cpp
    key_event_test.cpp
    mock_libinput.cpp
)
add_executable(testLibinputKeyEvent ${testLibinputKeyEvent_SRCS})
target_link_libraries(testLibinputKeyEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore)
add_test(NAME kwin-testLibinputKeyEvent COMMAND testLibinputKeyEvent)
ecm_mark_as_test(testLibinputKeyEvent)

########################################################
# Test Pointer Event
########################################################
set(testLibinputPointerEvent_SRCS
    ../../libinput/device.cpp
    ../../libinput/events.cpp
    mock_libinput.cpp
    pointer_event_test.cpp
)
add_executable(testLibinputPointerEvent ${testLibinputPointerEvent_SRCS})
target_link_libraries(testLibinputPointerEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore)
add_test(NAME kwin-testLibinputPointerEvent COMMAND testLibinputPointerEvent)
ecm_mark_as_test(testLibinputPointerEvent)

########################################################
# Test Touch Event
########################################################
set(testLibinputTouchEvent_SRCS
    ../../libinput/device.cpp
    ../../libinput/events.cpp
    mock_libinput.cpp
    touch_event_test.cpp
)
add_executable(testLibinputTouchEvent ${testLibinputTouchEvent_SRCS})
target_link_libraries(testLibinputTouchEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore)
add_test(NAME kwin-testLibinputTouchEvent COMMAND testLibinputTouchEvent)
ecm_mark_as_test(testLibinputTouchEvent)

########################################################
# Test Gesture Event
########################################################
set(testLibinputGestureEvent_SRCS
    ../../libinput/device.cpp
    ../../libinput/events.cpp
    gesture_event_test.cpp
    mock_libinput.cpp
)
add_executable(testLibinputGestureEvent ${testLibinputGestureEvent_SRCS})
target_link_libraries(testLibinputGestureEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore)
add_test(NAME kwin-testLibinputGestureEvent COMMAND testLibinputGestureEvent)
ecm_mark_as_test(testLibinputGestureEvent)

########################################################
# Test Switch Event
########################################################
set(testLibinputSwitchEvent_SRCS
        ../../libinput/device.cpp
        ../../libinput/events.cpp
        mock_libinput.cpp
        switch_event_test.cpp
)
add_executable(testLibinputSwitchEvent ${testLibinputSwitchEvent_SRCS})
target_link_libraries(testLibinputSwitchEvent Qt5::Test Qt5::DBus Qt5::Widgets KF5::ConfigCore)
add_test(NAME kwin-testLibinputSwitchEvent COMMAND testLibinputSwitchEvent)
ecm_mark_as_test(testLibinputSwitchEvent)

########################################################
# Test Context
########################################################
set(testLibinputContext_SRCS
        ../../libinput/context.cpp
        ../../libinput/device.cpp
        ../../libinput/events.cpp
        ../../libinput/libinput_logging.cpp
        ../../logind.cpp
        context_test.cpp
        mock_libinput.cpp
        mock_udev.cpp
)
add_executable(testLibinputContext ${testLibinputContext_SRCS})
target_link_libraries(testLibinputContext
    KF5::ConfigCore
    KF5::WindowSystem
    Qt5::DBus
    Qt5::Test
    Qt5::Widgets
)
add_test(NAME kwin-testLibinputContext COMMAND testLibinputContext)
ecm_mark_as_test(testLibinputContext)

########################################################
# Test Input Events
########################################################
set(testInputEvents_SRCS input_event_test.cpp mock_libinput.cpp ../../libinput/device.cpp ../../input_event.cpp)
add_executable(testInputEvents ${testInputEvents_SRCS})
target_link_libraries(testInputEvents Qt5::Test Qt5::DBus Qt5::Gui Qt5::Widgets KF5::ConfigCore)
add_test(NAME kwin-testInputEvents COMMAND testInputEvents)
ecm_mark_as_test(testInputEvents)
