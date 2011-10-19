# Adds effect plugin with given name. Sources are given after the name
macro(KWIN4_ADD_EFFECT name)
    kde4_add_plugin(kwin4_effect_${name} ${ARGN})
    target_link_libraries(kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS} ${X11_LIBRARIES} ${QT_QTWEBKIT_LIBRARY} kephal)
    install(TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT)

macro(KWIN4_ADD_EFFECT_CONFIG name)
    kde4_add_plugin(kcm_kwin4_effect_${name} ${ARGN})
    target_link_libraries(kcm_kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kcm_kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT_CONFIG)

### tests - meant to test/demo certain functionality, not really usable for end-users
SET(kwin4_effect_tests_sources
    demo_shakymove.cpp
    demo_shiftworkspaceup.cpp
    drunken.cpp
    flame.cpp
    howto.cpp
    test_input.cpp
    test_thumbnail.cpp
    kicker/kicker.cpp
    )
install( FILES
    demo_shakymove.desktop
    demo_shiftworkspaceup.desktop
    drunken.desktop
    flame.desktop
    howto.desktop
    test_input.desktop
    test_thumbnail.desktop
    kicker/kicker.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

if(OPENGL_FOUND)
    # opengl test/demo stuff
    SET(kwin4_effect_tests_sources ${kwin4_effect_tests_sources}
        demo_liquid.cpp
        demo_showpicture.cpp
        demo_wavywindows.cpp
        ../cube/cube_proxy.cpp
        ../cube/cube.cpp
        gears.cpp
        test_fbo.cpp
        )

    install( FILES
        demo_liquid.desktop
        demo_showpicture.desktop
        demo_wavywindows.desktop
        gears.desktop
        test_fbo.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

    install( FILES
        data/liquid.frag
        data/liquid.vert
        DESTINATION  ${DATA_INSTALL_DIR}/kwin )
endif(OPENGL_FOUND)

# add the plugin
KWIN4_ADD_EFFECT(tests ${kwin4_effect_tests_sources})


### videocapture plugin
include(UsePkgConfig)
PKGCONFIG(libcaptury CAPTURY_INCLUDES CAPTURY_LINK_DIR CAPTURY_LDFLAGS CAPTURY_CFLAGS)
if( CAPTURY_LDFLAGS )
    SET( CAPTURY_FOUND TRUE )
endif( CAPTURY_LDFLAGS )

macro_log_feature(CAPTURY_FOUND "Captury framework library" "Realtime video capturing framework e.g. screen casts" "http://gitorious.org/projects/libcaptury" FALSE "0.3.0" "Provides for video recording desktop effects.")

macro_bool_to_01( CAPTURY_FOUND HAVE_CAPTURY )
if( CAPTURY_FOUND )
    KWIN4_ADD_EFFECT(videorecord videorecord.cpp)
    KWIN4_ADD_EFFECT_CONFIG(videorecord videorecord_config.cpp)
    target_link_libraries(kwin4_effect_videorecord ${KDE4_KIO_LIBS} ${CAPTURY_LDFLAGS})
    install( FILES videorecord.desktop videorecord_config.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
endif( CAPTURY_FOUND )
