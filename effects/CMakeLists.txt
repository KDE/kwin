# Adds effect plugin with given name. Sources are given after the name
macro(KWIN4_ADD_EFFECT name)
    kde4_automoc(${ARGN})
    kde4_add_plugin(kwin4_effect_${name} ${ARGN})
    target_link_libraries(kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT)

macro(KWIN4_ADD_EFFECT_CONFIG name)
    kde4_automoc(${ARGN})
    kde4_add_plugin(kcm_kwin4_effect_${name} ${ARGN})
    target_link_libraries(kcm_kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kcm_kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT_CONFIG)

include_directories(
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/lib
    )

install( FILES kwineffect.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR})



### builtins - most important, ready-to-use effects
# sources
SET(kwin4_effect_builtins_sources
    boxswitch.cpp
    desktopgrid.cpp
    dialogparent.cpp
    diminactive.cpp
    fade.cpp
    fallapart.cpp
    flame.cpp
    maketransparent.cpp
    minimizeanimation.cpp
    presentwindows.cpp
    scalein.cpp
    thumbnailaside.cpp
    zoom.cpp
    )
# their .desktop files
install( FILES
    boxswitch.desktop
    desktopgrid.desktop
    dialogparent.desktop
    diminactive.desktop
    fade.desktop
    fallapart.desktop
    flame.desktop
    maketransparent.desktop
    minimizeanimation.desktop
    presentwindows.desktop
    scalein.desktop
    thumbnailaside.desktop
    zoom.desktop
    DESTINATION ${SERVICES_INSTALL_DIR}/kwin )

if(OPENGL_FOUND)
    # opengl-based effects
    SET(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
        blur.cpp
        explosioneffect.cpp
        invert.cpp
        magnifier.cpp
        mousemark.cpp
        shadow.cpp
        sharpen.cpp
        trackmouse.cpp
        )
    install( FILES
        blur.desktop
        explosion.desktop
        invert.desktop
        magnifier.desktop
        mousemark.desktop
        shadow.desktop
        sharpen.desktop
        trackmouse.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
    install( FILES
        data/trackmouse.png
        data/explosion.frag
        data/explosion.vert
        data/explosion-start.png
        data/explosion-end.png
        data/blur.frag
        data/blur.vert
        data/blur-render.frag
        data/blur-render.vert
        data/invert.frag
        data/invert.vert
        data/sharpen.frag
        data/sharpen.vert
        DESTINATION  ${DATA_INSTALL_DIR}/kwin )

    # config modules
    KWIN4_ADD_EFFECT_CONFIG( builtins presentwindows_config.cpp shadow_config.cpp )
    install( FILES
        presentwindows_config.desktop
        shadow_config.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin )


endif(OPENGL_FOUND)

# showfps - need both xrender and opengl
if( OPENGL_FOUND AND X11_Xrender_FOUND )
    SET(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
        showfps.cpp
        )
    install( FILES
        showfps.desktop
        DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
endif( OPENGL_FOUND AND X11_Xrender_FOUND )

# add the plugin
KWIN4_ADD_EFFECT(builtins ${kwin4_effect_builtins_sources})
# link to xrender if necessary
# note that libkwineffects already links to opengl, so no need to add this here
if (X11_Xrender_FOUND)
  target_link_libraries(kwin4_effect_builtins ${X11_Xrender_LIB})
endif (X11_Xrender_FOUND)



### tests - meant to test/demo certain functionality, not really usable for end-users
SET(kwin4_effect_tests_sources
    demo_shakymove.cpp
    demo_shiftworkspaceup.cpp
    demo_taskbarthumbnail.cpp
    drunken.cpp
    howto.cpp
    test_input.cpp
    test_thumbnail.cpp
    )
install( FILES
    demo_shakymove.desktop
    demo_shiftworkspaceup.desktop
    demo_taskbarthumbnail.desktop
    drunken.desktop
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



### videocapture plugin
include(UsePkgConfig)
PKGCONFIG(libcaptury CAPTURY_INCLUDES CAPTURY_LINK_DIR CAPTURY_LDFLAGS CAPTURY_CFLAGS)
if( CAPTURY_LDFLAGS )
    SET( CAPTURY_FOUND TRUE )
endif( CAPTURY_LDFLAGS )
macro_bool_to_01( CAPTURY_FOUND HAVE_CAPTURY )
if( HAVE_CAPTURY )
    KWIN4_ADD_EFFECT(videorecord videorecord.cpp)
    target_link_libraries(kwin4_effect_videorecord ${CAPTURY_LDFLAGS})
    install( FILES videorecord.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kwin )
endif( HAVE_CAPTURY )

