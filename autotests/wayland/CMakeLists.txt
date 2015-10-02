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
