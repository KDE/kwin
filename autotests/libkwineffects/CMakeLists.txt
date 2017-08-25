include(ECMMarkAsTest)

macro(KWINEFFECTS_UNIT_TESTS)
   foreach(_testname ${ARGN})
      add_executable(${_testname} ${_testname}.cpp)
      add_test(NAME kwineffects-${_testname} COMMAND ${_testname})
      target_link_libraries(${_testname} Qt5::Test kwineffects)
      ecm_mark_as_test(${_testname})
   endforeach()
endmacro()

kwineffects_unit_tests(
    windowquadlisttest
)

add_executable(kwinglplatformtest kwinglplatformtest.cpp mock_gl.cpp ../../libkwineffects/kwinglplatform.cpp)
add_test(NAME kwineffects-kwinglplatformtest COMMAND kwinglplatformtest)
target_link_libraries(kwinglplatformtest Qt5::Test Qt5::Gui Qt5::X11Extras KF5::ConfigCore XCB::XCB)
ecm_mark_as_test(kwinglplatformtest)
