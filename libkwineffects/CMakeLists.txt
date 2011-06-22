########### next target ###############

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    kwinglobals.cpp
    kwineffects.cpp
    kwinglutils.cpp
    kwingltexture.cpp
    kwinglutils_funcs.cpp
    kwinglplatform.cpp
    kwinxrenderutils.cpp
    )

kde4_add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
target_link_libraries(kwineffects ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY}
    ${X11_LIBRARIES} kephal)
set_target_properties(kwineffects PROPERTIES VERSION 1.0.0 SOVERSION 1 )

install(TARGETS kwineffects EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

if(OPENGL_FOUND AND NOT KWIN_HAVE_OPENGLES_COMPOSITING)
  target_link_libraries(kwineffects ${OPENGL_gl_LIBRARY})
  target_link_libraries(kwineffects LINK_INTERFACE_LIBRARIES ${OPENGL_gl_LIBRARY})
# -ldl used by OpenGL code
  find_library(DL_LIBRARY dl)
  if (DL_LIBRARY)
    target_link_libraries(kwineffects ${DL_LIBRARY})
  endif(DL_LIBRARY)
  include_directories(${OPENGL_INCLUDE_DIR})
endif(OPENGL_FOUND AND NOT KWIN_HAVE_OPENGLES_COMPOSITING)
if(KWIN_HAVE_OPENGLES_COMPOSITING)
  target_link_libraries(kwineffects ${OPENGLES_LIBRARIES} ${OPENGLES_EGL_LIBRARIES})
  target_link_libraries(kwineffects LINK_INTERFACE_LIBRARIES ${OPENGLES_LIBRARIES} ${OPENGLES_EGL_LIBRARIES})
  include_directories(${OPENGLES_INCLUDE_DIR})
endif(KWIN_HAVE_OPENGLES_COMPOSITING)
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


install( FILES
    kwinglobals.h
    kwineffects.h
    kwinglplatform.h
    kwinglutils.h
    kwinglutils_funcs.h
    kwingltexture.h
    kwinxrenderutils.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinconfig.h
    DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel)
