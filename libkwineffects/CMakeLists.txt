########### next target ###############

###  effects lib  ###
set(kwin_EFFECTSLIB_SRCS
    kwineffects.cpp
    anidata.cpp
    kwinanimationeffect.cpp
    kwinxrenderutils.cpp
    )

set(kwineffects_QT_LIBS
    Qt5::DBus
    Qt5::Widgets
    Qt5::X11Extras
)

set(kwineffects_KDE_LIBS
    KF5::KConfigCore
)

set(kwineffects_XCB_LIBS
    ${XCB_XCB_LIBRARY}
    ${XCB_XFIXES_LIBRARY}
    ${XCB_RENDER_LIBRARY}
)

add_library(kwineffects SHARED ${kwin_EFFECTSLIB_SRCS})
target_link_libraries(kwineffects
    ${kwineffects_QT_LIBS}
    ${kwineffects_KDE_LIBS}
    ${kwineffects_XCB_LIBS}
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
    kwinglcolorcorrection.cpp
    )

macro( KWIN4_ADD_GLUTILS_BACKEND name glinclude )
    include_directories(${glinclude})
    if(OPENGL_EGL_FOUND)
        include_directories(${OPENGLES_EGL_INCLUDE_DIR})
    endif()
    add_library(${name} SHARED ${kwin_GLUTILSLIB_SRCS})
    target_link_libraries(${name} PUBLIC Qt5::DBus Qt5::X11Extras ${XCB_XCB_LIBRARY})
    if(OPENGL_EGL_FOUND)
       target_link_libraries(${name} PRIVATE ${OPENGLES_EGL_LIBRARY})
    endif()
    set_target_properties(${name} PROPERTIES VERSION 1.0.0 SOVERSION 1 )
    target_link_libraries(${name} PUBLIC ${ARGN})

    install(TARGETS ${name} EXPORT kdeworkspaceLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})
endmacro()

if(KWIN_BUILD_OPENGLES)
    kwin4_add_glutils_backend(kwinglesutils ${OPENGLES_INCLUDE_DIR} ${OPENGLES_LIBRARIES})
    set_target_properties(kwinglesutils PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGLES)
    set_target_properties(kwinglesutils PROPERTIES OUTPUT_NAME ${KWIN_NAME}glesutils)
endif()
if(KWIN_BUILD_OPENGL)
    kwin4_add_glutils_backend(kwinglutils ${OPENGL_INCLUDE_DIR} ${OPENGL_gl_LIBRARY})
    set_target_properties(kwinglutils PROPERTIES OUTPUT_NAME ${KWIN_NAME}glutils)

    if( KWIN_BUILD_OPENGL_1_COMPOSITING )
        set_target_properties(kwinglutils PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGL_1)
    endif()

    target_link_libraries(kwinglutils PUBLIC ${OPENGL_gl_LIBRARY})
    # -ldl used by OpenGL code
    find_library(DL_LIBRARY dl)
    if (DL_LIBRARY)
       target_link_libraries(kwinglutils PRIVATE ${DL_LIBRARY})
    endif()
endif()

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

add_subdirectory(autotests)
