########################################################
# Test Devices
########################################################
set( testLibinputDevice_SRCS device_test.cpp mock_libinput.cpp ../../libinput/device.cpp )
add_executable(testLibinputDevice ${testLibinputDevice_SRCS})
target_link_libraries( testLibinputDevice Qt5::Test)
add_test(kwin-testLibinputDevice testLibinputDevice)
ecm_mark_as_test(testLibinputDevice)

########################################################
# Test Input Events
########################################################
set( testInputEvents_SRCS input_event_test.cpp mock_libinput.cpp ../../libinput/device.cpp ../../input_event.cpp )
add_executable(testInputEvents ${testInputEvents_SRCS})
target_link_libraries( testInputEvents Qt5::Test Qt5::Gui)
add_test(kwin-testInputEvents testInputEvents)
ecm_mark_as_test(testInputEvents)
