
configure_file(kwinconfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kwinconfig.h )


########### next target ###############

set(kdecorations_LIB_SRCS 
   kdecoration.cpp 
   kdecoration_p.cpp 
   kdecoration_plugins_p.cpp 
   kdecorationfactory.cpp 
   kcommondecoration.cpp
   kcommondecoration_p.cpp )


kde4_add_library(kdecorations SHARED ${kdecorations_LIB_SRCS})

target_link_libraries(kdecorations  ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY})

set_target_properties(kdecorations PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION}  )
install(TARGETS kdecorations  DESTINATION ${LIB_INSTALL_DIR} )


########### install files ###############

install( FILES kdecoration.h kdecorationfactory.h kcommondecoration.h DESTINATION ${INCLUDE_INSTALL_DIR})

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    kwinglobals.cpp
    kwineffects.cpp
    kwinglutils.cpp
    kwinglutils_funcs.cpp
    kwinshadereffect.cpp
    )
kde4_add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
target_link_libraries(kwineffects ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY} ${X11_LIBRARIES})
set_target_properties(kwineffects PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS kwineffects DESTINATION ${LIB_INSTALL_DIR})

if(OPENGL_FOUND)
  target_link_libraries(kwineffects ${OPENGL_gl_LIBRARY})
# -ldl used by OpenGL code
  find_library(DL_LIBRARY dl)
  if (DL_LIBRARY)
    target_link_libraries(kwineffects ${DL_LIBRARY})
  endif(DL_LIBRARY)
endif(OPENGL_FOUND)
if (X11_Xrender_FOUND)
  target_link_libraries(kwineffects ${X11_Xrender_LIB})
endif (X11_Xrender_FOUND)
if (X11_Xrandr_FOUND)
  target_link_libraries(kwineffects ${X11_Xrandr_LIB})
endif (X11_Xrandr_FOUND)
if (X11_Xcomposite_FOUND)
  target_link_libraries(kwineffects ${X11_Xcomposite_LIB})
endif (X11_Xcomposite_FOUND)
if (X11_Xdamage_FOUND)
  target_link_libraries(kwineffects ${X11_Xdamage_LIB})
endif (X11_Xdamage_FOUND)
if (X11_Xfixes_FOUND)
  target_link_libraries(kwineffects ${X11_Xfixes_LIB})
endif (X11_Xfixes_FOUND)


install( FILES kwinglobals.h kwineffects.h DESTINATION ${INCLUDE_INSTALL_DIR})

