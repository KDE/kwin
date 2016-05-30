########################################################
# Test Devices
########################################################
set( testLibinputDevice_SRCS device_test.cpp mock_libinput.cpp ../../libinput/device.cpp )
add_executable(testLibinputDevice ${testLibinputDevice_SRCS})
target_link_libraries( testLibinputDevice Qt5::Test)
add_test(kwin-testLibinputDevice testLibinputDevice)
ecm_mark_as_test(testLibinputDevice)

########################################################
# Test Key Event
########################################################
set( testLibinputKeyEvent_SRCS
        key_event_test.cpp
        mock_libinput.cpp
        ../../libinput/device.cpp
        ../../libinput/events.cpp
    )
add_executable(testLibinputKeyEvent ${testLibinputKeyEvent_SRCS})
target_link_libraries( testLibinputKeyEvent Qt5::Test Qt5::Widgets KF5::ConfigCore)
add_test(kwin-testLibinputKeyEvent testLibinputKeyEvent)
ecm_mark_as_test(testLibinputKeyEvent)

########################################################
# Test Input Events
########################################################
set( testInputEvents_SRCS input_event_test.cpp mock_libinput.cpp ../../libinput/device.cpp ../../input_event.cpp )
add_executable(testInputEvents ${testInputEvents_SRCS})
target_link_libraries( testInputEvents Qt5::Test Qt5::Gui)
add_test(kwin-testInputEvents testInputEvents)
ecm_mark_as_test(testInputEvents)
