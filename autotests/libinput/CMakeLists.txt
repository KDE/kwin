########################################################
# Test Devices
########################################################
set( testLibinputDevice_SRCS device_test.cpp mock_libinput.cpp ../../libinput/device.cpp )
add_executable(testLibinputDevice ${testLibinputDevice_SRCS})
target_link_libraries( testLibinputDevice Libinput::Libinput Qt5::Test)
add_test(kwin-testLibinputDevice testLibinputDevice)
ecm_mark_as_test(testLibinputDevice)
