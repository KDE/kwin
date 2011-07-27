########### next target ###############

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    kwinglobals.cpp
    kwineffects.cpp
    kwinxrenderutils.cpp
    )

kde4_add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
target_link_libraries(kwineffects ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY}
    ${X11_LIBRARIES} kephal)
set_target_properties(kwineffects PROPERTIES VERSION 1.0.0 SOVERSION 1 )

install(TARGETS kwineffects EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

if(OPENGL_FOUND OR OPENGLES_FOUND)

    set(kwin_GLUTILSLIB_SRCS
        kwinglutils.cpp
        kwingltexture.cpp
        kwinglutils_funcs.cpp
        kwinglplatform.cpp
        )

    macro( KWIN4_ADD_GLUTILS_BACKEND name glinclude )
        include_directories(${glinclude})
        kde4_add_library(${name} SHARED ${kwin_GLUTILSLIB_SRCS})
        target_link_libraries(${name} ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY} ${X11_LIBRARIES} kephal kwineffects)
        set_target_properties(${name} PROPERTIES VERSION 1.0.0 SOVERSION 1 )
        target_link_libraries(${name} ${ARGN})
        target_link_libraries(${name} LINK_INTERFACE_LIBRARIES ${ARGN})

        install(TARGETS ${name} EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})
    endmacro( KWIN4_ADD_GLUTILS_BACKEND )

    if(OPENGLES_FOUND)
        KWIN4_ADD_GLUTILS_BACKEND(kwinglesutils ${OPENGLES_INCLUDE_DIR} ${OPENGLES_LIBRARIES} ${OPENGLES_EGL_LIBRARIES})
        set_target_properties(kwinglesutils PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGL -DKWIN_HAVE_OPENGLES")
    endif(OPENGLES_FOUND)
    if(OPENGL_FOUND)
        KWIN4_ADD_GLUTILS_BACKEND(kwinglutils ${OPENGL_INCLUDE_DIR} ${OPENGL_gl_LIBRARY})
        set_target_properties(kwinglutils PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGL)

        target_link_libraries(kwinglutils ${OPENGL_gl_LIBRARY})
        target_link_libraries(kwinglutils LINK_INTERFACE_LIBRARIES ${OPENGL_gl_LIBRARY})
        # -ldl used by OpenGL code
        find_library(DL_LIBRARY dl)
        if (DL_LIBRARY)
            target_link_libraries(kwinglutils ${DL_LIBRARY})
        endif(DL_LIBRARY)
    endif(OPENGL_FOUND)

endif(OPENGL_FOUND OR OPENGLES_FOUND)

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
