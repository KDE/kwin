########### configure tests ###############
INCLUDE(CMakeDependentOption)

OPTION(KWIN_BUILD_DECORATIONS "Enable building of KWin decorations." ON)
OPTION(KWIN_BUILD_OXYGEN "Enable building of default decoration Oxygen" ON)
OPTION(KWIN_BUILD_KCMS "Enable building of KWin configuration modules." ON)
OPTION(KWIN_MOBILE_EFFECTS "Only build effects relevant for mobile devices" OFF)
OPTION(KWIN_BUILD_TABBOX "Enable building of KWin Tabbox functionality" ON)
OPTION(KWIN_BUILD_SCREENEDGES "Enable building of KWin with screen edge support" ON)
OPTION(KWIN_BUILD_SCRIPTING "Enable building of KWin with scripting support" ON)
OPTION(KWIN_BUILD_KAPPMENU "Enable building of KWin with application menu support" ON)
OPTION(KWIN_BUILD_XRENDER_COMPOSITING "Enable building of KWin with XRender Compositing support" ON)
OPTION(KWIN_BUILD_OPENGL_1_COMPOSITING "Enable support for OpenGL 1.x, automatically disabled when building for OpenGL ES 2.0" ON)
OPTION(KWIN_BUILD_ACTIVITIES "Enable building of KWin with kactivities support" ON)
if(${KDE_PLATFORM_PROFILE} STREQUAL "Desktop")
   OPTION(KWIN_PLASMA_ACTIVE "Enable building KWin for Plasma Active." OFF)
else()
   OPTION(KWIN_PLASMA_ACTIVE "Enable building KWin for Plasma Active." On)
endif()

# Binary name of KWin
set(KWIN_NAME "kwin")

if(KWIN_PLASMA_ACTIVE)
    set(KWIN_BUILD_DECORATIONS OFF)
    set(KWIN_BUILD_KCMS OFF)
    set(KWIN_BUILD_SCREENEDGES OFF)
    set(KWIN_BUILD_SCRIPTING ON)
    set(KWIN_BUILD_XRENDER_COMPOSITING OFF)
    set(KWIN_MOBILE_EFFECTS ON)
    set(KWIN_BUILD_WITH_OPENGLES ON)
    set(KWIN_NAME "kwinactive")
endif()

cmake_dependent_option(KWIN_BUILD_KAPPMENU "Build without appmenu support" ON "KWIN_BUILD_DECORATIONS" FALSE)

# KWIN_HAVE_XRENDER_COMPOSITING - whether XRender-based compositing support is available: may be disabled
if( KWIN_BUILD_XRENDER_COMPOSITING )
    set( KWIN_HAVE_XRENDER_COMPOSITING 1 )
endif()

if(OPENGL_FOUND)
  include_directories(${OPENGL_INCLUDE_DIR})
endif()

if(OPENGL_EGL_FOUND)
    include_directories(${OPENGLES_EGL_INCLUDE_DIR})
    set(KWIN_HAVE_EGL 1)
endif()

if(OPENGLES_FOUND)
  include_directories(${OPENGLES_INCLUDE_DIR})
endif()

include_directories(${XCB_INCLUDE_DIR})

if(WAYLAND_FOUND AND KWIN_HAVE_EGL)
    include_directories(${WAYLAND_INCLUDE_DIR})
endif()

# for things that are also used by kwin libraries
configure_file(libkwineffects/kwinconfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects/kwinconfig.h )
# for kwin internal things
configure_file(config-kwin.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h )


########### global ###############

include_directories(BEFORE
    ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libkwineffects
    ${CMAKE_CURRENT_SOURCE_DIR}/libkdecorations
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
    ${CMAKE_CURRENT_SOURCE_DIR}/tabbox
    )

add_subdirectory( libkdecorations )
add_subdirectory( libkwineffects )
add_subdirectory( killer )
if(KWIN_BUILD_KCMS)
    add_subdirectory( kcmkwin )
endif()

if( KWIN_BUILD_DECORATIONS )
    add_subdirectory( clients )
endif()
add_subdirectory( data )

add_subdirectory( effects )
add_subdirectory( scripts )
add_subdirectory( tabbox )

if(KWIN_BUILD_SCRIPTING)
    add_subdirectory(scripting)
endif()

########### next target ###############

set(kwin_KDEINIT_SRCS
   workspace.cpp 
   dbusinterface.cpp
   client.cpp 
   client_machine.cpp
   cursor.cpp
   tabgroup.cpp
   focuschain.cpp
   netinfo.cpp
   placement.cpp 
   atoms.cpp 
   utils.cpp 
   layers.cpp 
   main.cpp 
   options.cpp 
   outline.cpp
   decorations.cpp
   events.cpp 
   killwindow.cpp 
   geometrytip.cpp 
   screens.cpp
   shadow.cpp
   sm.cpp 
   group.cpp 
   bridge.cpp 
   manage.cpp 
   overlaywindow.cpp
   activation.cpp 
   useractions.cpp 
   geometry.cpp 
   rules.cpp
   composite.cpp
   toplevel.cpp
   unmanaged.cpp
   scene.cpp
   scene_xrender.cpp
   scene_opengl.cpp
   glxbackend.cpp
   thumbnailitem.cpp
   lanczosfilter.cpp
   deleted.cpp
   effects.cpp
   compositingprefs.cpp
   paintredirector.cpp
   virtualdesktops.cpp
   xcbutils.cpp
   )

if(KWIN_BUILD_SCRIPTING)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        scripting/scripting.cpp
        scripting/workspace_wrapper.cpp
        scripting/meta.cpp
        scripting/scriptedeffect.cpp
        scripting/scriptingutils.cpp
        scripting/timer.cpp
        scripting/scripting_model.cpp
    )
endif()

if(KWIN_BUILD_TABBOX)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        tabbox/tabbox.cpp
        tabbox/clientmodel.cpp
        tabbox/declarative.cpp
        tabbox/desktopchain.cpp
        tabbox/desktopmodel.cpp
        tabbox/tabboxconfig.cpp
        tabbox/tabboxhandler.cpp
  )
endif()

if(KWIN_BUILD_SCREENEDGES)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        screenedge.cpp
    )
endif()

if(KWIN_BUILD_KAPPMENU)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        appmenu.cpp
    )
endif()

if(KWIN_BUILD_ACTIVITIES)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        activities.cpp
    )
endif()

if(KWIN_HAVE_EGL)
    set(kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS} eglonxbackend.cpp)
endif()

if(WAYLAND_FOUND AND KWIN_HAVE_EGL)
    set(kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS} egl_wayland_backend.cpp)
endif()

kde4_add_kcfg_files(kwin_KDEINIT_SRCS settings.kcfgc)

qt4_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.KWin.xml dbusinterface.h KWin::DBusInterface )
qt4_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.kwin.Compositing.xml composite.h KWin::Compositor )
qt4_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.kwin.Effects.xml effects.h KWin::EffectsHandlerImpl )

qt4_add_dbus_interface( kwin_KDEINIT_SRCS
  ${KDEBASE_WORKSPACE_SOURCE_DIR}/ksmserver/org.kde.KSMServerInterface.xml ksmserver_interface)
qt4_add_dbus_interface( kwin_KDEINIT_SRCS
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/ksmserver/screenlocker/dbus/org.freedesktop.ScreenSaver.xml screenlocker_interface)

qt4_add_resources( kwin_KDEINIT_SRCS resources.qrc )

########### target link libraries ###############

set(kwin_OWN_LIBS
    kdecorations
    kwineffects
)

set(kwin_QT_LIBS
    ${QT_QTDECLARATIVE_LIBRARY}
)

set(kwin_KDE_LIBS
    ${KDE4_KDEUI_LIBS}
    ${KDE4_PLASMA_LIBS}
    ${KDECLARATIVE_LIBRARIES}
)

set(kwin_XLIB_LIBS
    ${X11_X11_LIB}
    ${X11_Xext_LIB}
    ${X11_ICE_LIB}
    ${X11_SM_LIB}
    ${X11_Xcursor_LIB}
    ${X11_Xrandr_LIB}
    ${X11_Xdamage_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xfixes_LIB}
)

set(kwin_XCB_LIBS
    ${XCB_XCB_LIBRARIES}
    ${X11_XCB_LIBRARIES}
    ${XCB_XFIXES_LIBRARIES}
    ${XCB_DAMAGE_LIBRARIES}
    ${XCB_COMPOSITE_LIBRARIES}
    ${XCB_SHAPE_LIBRARIES}
    ${XCB_SYNC_LIBRARIES}
    ${XCB_RENDER_LIBRARIES}
    ${XCB_RANDR_LIBRARIES}
    ${XCB_KEYSYMS_LIBRARIES}
)

set(kwin_WAYLAND_LIBS
    ${WAYLAND_CLIENT_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
    ${XCB_SHM_LIBRARIES}
    ${XCB_XTEST_LIBRARIES}
)

set(kwin_OPENGL_LIBS )

find_library(XF86VM_LIBRARY Xxf86vm)
if (XF86VM_LIBRARY)
    set(kwin_XLIB_LIBS ${kwin_XLIB_LIBS} ${XF86VM_LIBRARY})
else()
    add_definitions(-DKWIN_NO_XF86VM)
endif()

if(KWIN_BUILD_SCRIPTING)
    set(kwin_QT_LIBS ${kwin_QT_LIBS} ${QT_QTSCRIPT_LIBRARY})
endif()

if(KWIN_BUILD_ACTIVITIES)
    set(kwin_KDE_LIBS ${kwin_KDE_LIBS} ${KACTIVITIES_LIBRARY})
endif()

if(OPENGL_EGL_FOUND)
    set(kwin_OPENGL_LIBS ${kwin_OPENGL_LIBS} ${OPENGLES_EGL_LIBRARY})
endif()

set(kwinLibs
    ${kwin_OWN_LIBS}
    ${kwin_QT_LIBS}
    ${kwin_KDE_LIBS}
    ${kwin_XLIB_LIBS}
    ${kwin_XCB_LIBS}
    ${kwin_OPENGL_LIBS}
)

if(WAYLAND_FOUND  AND KWIN_HAVE_EGL)
    set(kwinLibs ${kwinLibs} ${kwin_WAYLAND_LIBS})
endif()

kde4_add_kdeinit_executable( kwin ${kwin_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin ${kwinLibs})
set_target_properties(kwin PROPERTIES OUTPUT_NAME ${KWIN_NAME})
set_target_properties(kdeinit_kwin PROPERTIES OUTPUT_NAME kdeinit4_${KWIN_NAME})

if(OPENGL_FOUND)
    if( KWIN_BUILD_OPENGL_1_COMPOSITING )
        set_target_properties(kdeinit_kwin PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGL_1)
    endif()
  add_subdirectory(opengltest)
  target_link_libraries(kdeinit_kwin kwinglutils ${OPENGL_gl_LIBRARY})
  # -ldl used by OpenGL code
  find_library(DL_LIBRARY dl)
  if (DL_LIBRARY)
    target_link_libraries(kdeinit_kwin ${DL_LIBRARY})
  endif()
elseif(OPENGLES_FOUND)
  target_link_libraries(kdeinit_kwin ${kwinLibs} kwinglesutils ${OPENGLES_LIBRARIES})
  set_target_properties(kdeinit_kwin PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGLES")
endif()

install(TARGETS kdeinit_kwin ${INSTALL_TARGETS_DEFAULT_ARGS} )
install(TARGETS kwin         ${INSTALL_TARGETS_DEFAULT_ARGS} )

if(OPENGLES_FOUND)
  kde4_add_kdeinit_executable( kwin_gles ${kwin_KDEINIT_SRCS})
  target_link_libraries(kdeinit_kwin_gles ${kwinLibs} kwinglesutils ${OPENGLES_LIBRARIES})
  set_target_properties(kdeinit_kwin_gles PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGLES")
  set_target_properties(kwin_gles PROPERTIES OUTPUT_NAME ${KWIN_NAME}_gles)
  set_target_properties(kdeinit_kwin_gles PROPERTIES OUTPUT_NAME kdeinit4_${KWIN_NAME}_gles)
  install(TARGETS kdeinit_kwin_gles ${INSTALL_TARGETS_DEFAULT_ARGS} )
  install(TARGETS kwin_gles         ${INSTALL_TARGETS_DEFAULT_ARGS} )
endif()

########### install files ###############

install( FILES kwin.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} RENAME ${KWIN_NAME}.kcfg )
install( FILES kwin.notifyrc  DESTINATION  ${DATA_INSTALL_DIR}/${KWIN_NAME} RENAME ${KWIN_NAME}.notifyrc)
install( FILES org.kde.KWin.xml DESTINATION ${DBUS_INTERFACES_INSTALL_DIR} )

if( KWIN_BUILD_SCRIPTING )
    # Install the KWin/WindowSwitcher service type
    install( FILES scripting/kwinscript.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )
endif()

kde4_install_icons( ${ICON_INSTALL_DIR} )

add_subdirectory(tests)
