integrationTest(NAME testSlidingPopups SRCS slidingpopups_test.cpp BUILTIN_EFFECTS)
integrationTest(NAME testScriptedEffects SRCS scripted_effects_test.cpp LIBS KF6::GlobalAccel BUILTIN_EFFECTS PROPERTIES RUN_SERIAL TRUE)
integrationTest(NAME testToplevelOpenCloseAnimation SRCS toplevel_open_close_animation_test.cpp BUILTIN_EFFECTS)
integrationTest(NAME testPopupOpenCloseAnimation SRCS popup_open_close_animation_test.cpp LIBS KF6::I18n KDecoration3::KDecoration3Private BUILTIN_EFFECTS)
integrationTest(NAME testDesktopSwitchingAnimation SRCS desktop_switching_animation_test.cpp BUILTIN_EFFECTS)
integrationTest(NAME testMinimizeAnimation SRCS minimize_animation_test.cpp BUILTIN_EFFECTS)
integrationTest(NAME testMaximizeAnimation SRCS maximize_animation_test.cpp BUILTIN_EFFECTS)

if(KWIN_BUILD_X11)
    integrationTest(NAME testTranslucency SRCS translucency_test.cpp LIBS XCB::ICCCM BUILTIN_EFFECTS)
endif()

add_library(externaleffect MODULE externaleffect/externaleffect.cpp)
target_link_libraries(externaleffect kwin)
set_target_properties(externaleffect PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kwin/effects/plugins"
)

foreach(restartCount 0 1 2)
    integrationTest(NAME testExternalEffects${restartCount} SRCS external_effects_test.cpp BUILTIN_EFFECTS)
    target_compile_definitions(testExternalEffects${restartCount} PRIVATE KWIN_TEST_RESTART_COUNT=${restartCount})
    add_dependencies(testExternalEffects${restartCount} externaleffect)
endforeach()
