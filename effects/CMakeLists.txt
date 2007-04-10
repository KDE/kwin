# Adds effect plugin with given name. Sources are given after the name
macro(KWIN4_ADD_EFFECT name)
    kde4_automoc(kwin4_effect_${name} ${ARGN})
    kde4_add_plugin(kwin4_effect_${name} ${ARGN})
    target_link_libraries(kwin4_effect_${name} kwineffects ${KDE4_KDEUI_LIBS})
    if(OPENGL_FOUND)
      target_link_libraries(kwin4_effect_${name} ${OPENGL_gl_LIBRARY})
    endif(OPENGL_FOUND)
    if (X11_Xrender_FOUND)
      target_link_libraries(kwin4_effect_${name} ${X11_Xrender_LIB})
    endif (X11_Xrender_FOUND)
    install(TARGETS kwin4_effect_${name} DESTINATION ${PLUGIN_INSTALL_DIR})
endmacro(KWIN4_ADD_EFFECT)


include_directories(
    ${CMAKE_SOURCE_DIR}/workspace/kwin/lib
    )

SET(kwin4_effect_builtins_sources
    presentwindows.cpp
    diminactive.cpp
    )

if(OPENGL_FOUND)
SET(kwin4_effect_builtins_sources ${kwin4_effect_builtins_sources}
    shadow.cpp
    showfps.cpp
    )
install( FILES
    shadow.desktop
    showfps.desktop
    DESTINATION ${DATA_INSTALL_DIR}/kwin/effects )
endif(OPENGL_FOUND)

if (X11_Xrender_FOUND)
endif (X11_Xrender_FOUND)

KWIN4_ADD_EFFECT(builtins ${kwin4_effect_builtins_sources})

install( FILES
    presentwindows.desktop
    diminactive.desktop
    DESTINATION ${DATA_INSTALL_DIR}/kwin/effects )
