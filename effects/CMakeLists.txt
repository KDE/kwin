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
    presentwindows.cpp
    diminactive.cpp
    zoom.cpp
    )

if(OPENGL_FOUND)
    SET(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
        shadow.cpp
        )
    install( FILES
        shadow.desktop
        DESTINATION ${DATA_INSTALL_DIR}/kwin/effects )
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

install( FILES
    presentwindows.desktop
    diminactive.desktop
    zoom.desktop
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
