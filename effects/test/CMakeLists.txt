# Adds effect plugin with given name. Sources are given after the name
macro(KWIN4_ADD_EFFECT name)
    kde4_add_plugin(kwin4_effect_${name} ${ARGN})
    target_link_libraries(kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT)

macro(KWIN4_ADD_EFFECT_CONFIG name)
    kde4_add_plugin(kcm_kwin4_effect_${name} ${ARGN})
    target_link_libraries(kcm_kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kcm_kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT_CONFIG)

include_directories(
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib
    )


### tests - meant to test/demo certain functionality, not really usable for end-users
SET(kwin4_effect_tests_sources
    demo_shakymove.cpp
    demo_shiftworkspaceup.cpp
    drunken.cpp
    flame.cpp
    howto.cpp
    test_input.cpp
    test_thumbnail.cpp
    )
install( FILES
    demo_shakymove.desktop
    demo_shiftworkspaceup.desktop
    drunken.desktop
    flame.desktop
    howto.desktop
    test_input.desktop
    test_thumbnail.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

if(OPENGL_FOUND)
    # opengl test/demo stuff
    SET(kwin4_effect_tests_sources ${kwin4_effect_tests_sources}
        demo_liquid.cpp
        demo_showpicture.cpp
        demo_wavywindows.cpp
        test_fbo.cpp
        )

    install( FILES
        demo_liquid.desktop
        demo_showpicture.desktop
        demo_wavywindows.desktop
        test_fbo.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

    install( FILES
        data/liquid.frag
        data/liquid.vert
        DESTINATION  ${DATA_INSTALL_DIR}/kwin )
endif(OPENGL_FOUND)

# add the plugin
KWIN4_ADD_EFFECT(tests ${kwin4_effect_tests_sources})

