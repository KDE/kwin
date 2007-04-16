# Adds effect plugin with given name. Sources are given after the name
macro(KWIN4_ADD_EFFECT name)
    kde4_automoc(kwin4_effect_${name} ${ARGN})
    kde4_add_plugin(kwin4_effect_${name} ${ARGN})
    target_link_libraries(kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    install(TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT)

include_directories(
    ${CMAKE_SOURCE_DIR}/workspace/kwin/lib
    )

SET(kwin4_effect_builtins_sources
    boxswitch.cpp
    desktopgrid.cpp
    dialogparent.cpp
    diminactive.cpp
    drunken.cpp
    fade.cpp
    fallapart.cpp
    flame.cpp
    maketransparent.cpp
    minimizeanimation.cpp
    presentwindows.cpp
    scalein.cpp
    shakymove.cpp
    thumbnailaside.cpp
    zoom.cpp
    )

SET(kwin4_effect_tests_sources
    demo_shiftworkspaceup.cpp
    demo_showpicture.cpp
    demo_taskbarthumbnail.cpp
    howto.cpp
    test_input.cpp
    test_thumbnail.cpp
    )

if(OPENGL_FOUND)
    SET(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
        magnifier.cpp
        shadow.cpp
        trackmouse.cpp
        wavywindows.cpp
        )
    install( FILES
        magnifier.desktop
        shadow.desktop
        trackmouse.desktop
        wavywindows.desktop
        DESTINATION ${DATA_INSTALL_DIR}/kwin/effects )

    install( FILES
        data/trackmouse.png
        data/explosion.frag
        data/explosion.vert
        data/explosion-start.png
        data/explosion-end.png
        DESTINATION  ${DATA_INSTALL_DIR}/kwin )
endif(OPENGL_FOUND)

if (X11_Xrender_FOUND)
endif (X11_Xrender_FOUND)

if( OPENGL_FOUND AND X11_Xrender_FOUND )
    SET(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
        showfps.cpp
        )
    install( FILES
        showfps.desktop
        DESTINATION ${DATA_INSTALL_DIR}/kwin/effects )
endif( OPENGL_FOUND AND X11_Xrender_FOUND )

KWIN4_ADD_EFFECT(builtins ${kwin4_effect_builtins_sources})
if(OPENGL_FOUND)
  target_link_libraries(kwin4_effect_builtins ${OPENGL_gl_LIBRARY})
endif(OPENGL_FOUND)
if (X11_Xrender_FOUND)
  target_link_libraries(kwin4_effect_builtins ${X11_Xrender_LIB})
endif (X11_Xrender_FOUND)

KWIN4_ADD_EFFECT(tests ${kwin4_effect_tests_sources})

install( FILES
    boxswitch.desktop
    desktopgrid.desktop
    dialogparent.desktop
    diminactive.desktop
    drunken.desktop
    fade.desktop
    fallapart.desktop
    flame.desktop
    howto.desktop
    magnifier.desktop
    maketransparent.desktop
    minimizeanimation.desktop
    presentwindows.desktop
    scalein.desktop
    shakymove.desktop
    thumbnailaside.desktop
    zoom.desktop
    demo_shiftworkspaceup.desktop
    demo_showpicture.desktop
    demo_taskbarthumbnail.desktop
    test_input.desktop
    test_thumbnail.desktop
    DESTINATION ${DATA_INSTALL_DIR}/kwin/effects )

include(UsePkgConfig)
PKGCONFIG(libcaptury CAPTURY_INCLUDES CAPTURY_LINK_DIR CAPTURY_LDFLAGS CAPTURY_CFLAGS)
if( CAPTURY_LDFLAGS )
    SET( CAPTURY_FOUND TRUE )
endif( CAPTURY_LDFLAGS )
macro_bool_to_01( CAPTURY_FOUND HAVE_CAPTURY )
if( HAVE_CAPTURY )
    KWIN4_ADD_EFFECT(videorecord videorecord.cpp)
    target_link_libraries(kwin4_effect_videorecord ${CAPTURY_LDFLAGS})
    install( FILES videorecord.desktop DESTINATION ${DATA_INSTALL_DIR}/kwin/effects )
endif( HAVE_CAPTURY )

