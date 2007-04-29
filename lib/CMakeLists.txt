


########### next target ###############

set(kdecorations_LIB_SRCS 
   kdecoration.cpp 
   kdecoration_p.cpp 
   kdecoration_plugins_p.cpp 
   kdecorationfactory.cpp 
   kcommondecoration.cpp )

kde4_automoc(kdecorations ${kdecorations_LIB_SRCS})

kde4_add_library(kdecorations SHARED ${kdecorations_LIB_SRCS})

target_link_libraries(kdecorations  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

set_target_properties(kdecorations PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS kdecorations  DESTINATION ${LIB_INSTALL_DIR} )


########### install files ###############

install_files( /include FILES kdecoration.h kdecoration_p.h kdecoration_plugins_p.h          kdecorationfactory.h kcommondecoration.h )

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    kwinglobals.cpp
    kwineffects.cpp
    kwinglutils.cpp
    kwinglutils_funcs.cpp
    )
kde4_automoc(${kwin_EFFECTSLIB_SRCS})
kde4_add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
target_link_libraries(kwineffects ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY} ${X11_LIBRARIES})
set_target_properties(kdecorations PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS kwineffects DESTINATION ${LIB_INSTALL_DIR})

if(OPENGL_FOUND)
  target_link_libraries(kwineffects ${OPENGL_gl_LIBRARY})
# -ldl used by OpenGL code
  target_link_libraries(kwineffects -ldl)
endif(OPENGL_FOUND)

install( FILES kwinglobals.h kwineffects.h kwinglutils.h kwinglutils_funcs.h DESTINATION ${INCLUDE_INSTALL_DIR})

