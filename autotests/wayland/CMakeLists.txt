add_definitions(-DKWINBACKENDPATH="${CMAKE_BINARY_DIR}/backends/virtual/KWinWaylandVirtualBackend.so")
add_definitions(-DKWINQPAPATH="${CMAKE_BINARY_DIR}/plugins/qpa/")
########################################################
# Test Start
########################################################
set( testStart_SRCS start_test.cpp kwin_wayland_test.cpp )
add_executable(testStart ${testStart_SRCS})
target_link_libraries( testStart kwin Qt5::Test)
add_test(kwin-testStart testStart)
ecm_mark_as_test(testStart)

########################################################
# Transient Window without input test
########################################################
set( testTransientNoInput_SRCS transient_no_input_test.cpp kwin_wayland_test.cpp )
add_executable(testTransientNoInput ${testTransientNoInput_SRCS})
target_link_libraries( testTransientNoInput kwin Qt5::Test)
add_test(kwin-testTransientNoInput testTransientNoInput)
ecm_mark_as_test(testTransientNoInput)
