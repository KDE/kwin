########### next target ###############

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    kwinglobals.cpp
    kwineffects.cpp
    anidata.cpp
    kwinanimationeffect.cpp
    kwinxrenderutils.cpp
    )

kde4_add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
target_link_libraries(kwineffects ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY}
    ${X11_LIBRARIES}
    ${X11_Xrender_LIB}
    ${X11_Xrandr_LIB}
    ${X11_Xcomposite_LIB}
    ${X11_Xdamage_LIB}
    ${X11_Xfixes_LIB}
)
set_target_properties(kwineffects PROPERTIES VERSION 1.0.0 SOVERSION 1 )
set_target_properties(kwineffects PROPERTIES OUTPUT_NAME ${KWIN_NAME}effects)

install(TARGETS kwineffects EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

# kwingl(es)utils library
set(kwin_GLUTILSLIB_SRCS
    kwinglutils.cpp
    kwingltexture.cpp
    kwinglutils_funcs.cpp
    kwinglplatform.cpp
    )

macro( KWIN4_ADD_GLUTILS_BACKEND name glinclude )
    include_directories(${glinclude})
    kde4_add_library(${name} SHARED ${kwin_GLUTILSLIB_SRCS})
    target_link_libraries(${name} ${KDE4_KDEUI_LIBS} ${QT_QTGUI_LIBRARY} ${X11_LIBRARIES} kwineffects)
    set_target_properties(${name} PROPERTIES VERSION 1.0.0 SOVERSION 1 )
    target_link_libraries(${name} ${ARGN})
    target_link_libraries(${name} LINK_INTERFACE_LIBRARIES ${ARGN})

    install(TARGETS ${name} EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})
endmacro( KWIN4_ADD_GLUTILS_BACKEND )

if(OPENGLES_FOUND)
    KWIN4_ADD_GLUTILS_BACKEND(kwinglesutils ${OPENGLES_INCLUDE_DIR} ${OPENGLES_LIBRARIES})
    set_target_properties(kwinglesutils PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGLES)
    set_target_properties(kwinglesutils PROPERTIES OUTPUT_NAME ${KWIN_NAME}glesutils)
endif(OPENGLES_FOUND)
if(OPENGL_FOUND)
    KWIN4_ADD_GLUTILS_BACKEND(kwinglutils ${OPENGL_INCLUDE_DIR} ${OPENGL_gl_LIBRARY})
    set_target_properties(kwinglutils PROPERTIES OUTPUT_NAME ${KWIN_NAME}glutils)

    target_link_libraries(kwinglutils ${OPENGL_gl_LIBRARY})
    target_link_libraries(kwinglutils LINK_INTERFACE_LIBRARIES ${OPENGL_gl_LIBRARY})
    # -ldl used by OpenGL code
    find_library(DL_LIBRARY dl)
    if (DL_LIBRARY)
        target_link_libraries(kwinglutils ${DL_LIBRARY})
    endif(DL_LIBRARY)
endif(OPENGL_FOUND)

install( FILES
    kwinglobals.h
    kwineffects.h
    kwinanimationeffect.h
    kwinglplatform.h
    kwinglutils.h
    kwinglutils_funcs.h
    kwingltexture.h
    kwinxrenderutils.h
    ${CMAKE_CURRENT_BINARY_DIR}/kwinconfig.h
    DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel)
