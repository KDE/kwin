########### configure tests ###############

# KWIN_HAVE_COMPOSITING - whether any compositing support is available
if( X11_Xcomposite_FOUND AND X11_Xdamage_FOUND )
    set( KWIN_HAVE_COMPOSITING 1 )
endif( X11_Xcomposite_FOUND AND X11_Xdamage_FOUND )

# KWIN_HAVE_OPENGL_COMPOSITING - whether OpenGL-based compositing support is available
if( KWIN_HAVE_COMPOSITING AND OPENGL_FOUND )
    set( KWIN_HAVE_OPENGL_COMPOSITING 1 )
endif( KWIN_HAVE_COMPOSITING AND OPENGL_FOUND )

# KWIN_HAVE_XRENDER_COMPOSITING - whether XRender-based compositing support is available
if( KWIN_HAVE_COMPOSITING AND X11_Xrender_FOUND AND X11_Xfixes_FOUND )
    set( KWIN_HAVE_XRENDER_COMPOSITING 1 )
endif( KWIN_HAVE_COMPOSITING AND X11_Xrender_FOUND AND X11_Xfixes_FOUND )

# KWIN_HAVE_OPENGLES_COMPOSITING - whether OpenGL ES-based compositing support is available
if( KWIN_HAVE_COMPOSITING AND OPENGLES_FOUND AND KWIN_BUILD_WITH_OPENGLES)
    set( KWIN_HAVE_OPENGL_COMPOSITING 1)
    set( KWIN_HAVE_OPENGLES_COMPOSITING 1 )
    message("Compiling KWin for mobile.")
endif( KWIN_HAVE_COMPOSITING AND OPENGLES_FOUND AND KWIN_BUILD_WITH_OPENGLES )

# safety
if( KWIN_HAVE_OPENGL_COMPOSITING OR KWIN_HAVE_XRENDER_COMPOSITING )
    # ok
else( KWIN_HAVE_OPENGL_COMPOSITING OR KWIN_HAVE_XRENDER_COMPOSITING )
    set( KWIN_HAVE_COMPOSITING ) # unset
endif( KWIN_HAVE_OPENGL_COMPOSITING OR KWIN_HAVE_XRENDER_COMPOSITING )

macro_log_feature(KWIN_HAVE_COMPOSITING "Compositing support" "X11 Compositing support"  "http://www.x.org/" FALSE "" "XComposite and XDamage extensions and OpenGL or XRender with XFixes are required for KWin compositing support")

macro_bool_to_01( OPENGL_FOUND KWIN_HAVE_OPENGL )
macro_bool_to_01( KWIN_HAVE_OPENGLES_COMPOSITING KWIN_HAVE_OPENGLES )

if(OPENGL_FOUND)
  include_directories(${OPENGL_INCLUDE_DIR})
endif(OPENGL_FOUND)

if(OPENGLES_FOUND AND KWIN_HAVE_OPENGLES_COMPOSITING)
  include_directories(${OPENGLES_INCLUDE_DIR})
endif(OPENGLES_FOUND AND KWIN_HAVE_OPENGLES_COMPOSITING)

OPTION(KWIN_BUILD_DECORATIONS "Enable building of KWin decorations." ON)
OPTION(KWIN_BUILD_KCMS "Enable building of KWin configuration modules." ON)
OPTION(KWIN_MOBILE_EFFECTS "Only build effects relevant for mobile devices" OFF)
OPTION(KWIN_BUILD_TABBOX "Enable building of KWin Tabbox functionality" ON)

# for things that are also used by kwin libraries
configure_file(libkwineffects/kwinconfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects/kwinconfig.h )
# for kwin internal things
configure_file(config-kwin.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h )


########### global ###############

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/libkwineffects
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libkwineffects
    ${CMAKE_CURRENT_SOURCE_DIR}/libkdecorations
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
    ${CMAKE_CURRENT_SOURCE_DIR}/tabbox
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/libs/kephal
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/libs/kworkspace
    )

add_subdirectory( libkdecorations )
add_subdirectory( libkwineffects )
add_subdirectory( killer )
if(KWIN_BUILD_KCMS)
    add_subdirectory( kcmkwin )
endif(KWIN_BUILD_KCMS)

if( KWIN_BUILD_DECORATIONS )
    add_subdirectory( clients )
endif( KWIN_BUILD_DECORATIONS )
add_subdirectory( data )

if( KWIN_HAVE_COMPOSITING )
add_subdirectory( effects )
endif( KWIN_HAVE_COMPOSITING )

########### next target ###############

set(kwin_KDEINIT_SRCS
   workspace.cpp 
   client.cpp 
   clientgroup.cpp
   placement.cpp 
   atoms.cpp 
   utils.cpp 
   layers.cpp 
   main.cpp 
   desktopchangeosd.cpp 
   options.cpp 
   outline.cpp
   plugins.cpp 
   events.cpp 
   killwindow.cpp 
   geometrytip.cpp 
   shadow.cpp
   sm.cpp 
   group.cpp 
   bridge.cpp 
   manage.cpp 
   notifications.cpp 
   activation.cpp 
   useractions.cpp 
   geometry.cpp 
   rules.cpp
   composite.cpp
   toplevel.cpp
   unmanaged.cpp
   scene.cpp
   scene_basic.cpp
   scene_xrender.cpp
   scene_opengl.cpp
   lanczosfilter.cpp
   deleted.cpp
   effects.cpp
   compositingprefs.cpp
   desktoplayout.cpp
   paintredirector.cpp
   screenedge.cpp
   tile.cpp
   tiling.cpp
   tilinglayout.cpp
   tilinglayoutfactory.cpp

   #load the scripting related functions
   scripting/scripting.cpp
   scripting/workspace.cpp
   scripting/client.cpp
   scripting/meta.cpp
   scripting/toplevel.cpp
   scripting/windowinfo.cpp
   scripting/s_clientgroup.cpp
   scripting/workspaceproxy.cpp
   scripting/chelate.cpp
   scripting/timer.cpp

   # tiling layouts
   # spiral
   #tilinglayouts/spiral/spiralfactory.cpp
   tilinglayouts/spiral/spiral.cpp

   # columns
   #tilinglayouts/columns/columnsfactory.cpp
   tilinglayouts/columns/columns.cpp

   # floating
   tilinglayouts/floating/floating.cpp
   )

if(KWIN_BUILD_TABBOX)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        tabbox.cpp
        tabbox/clientitemdelegate.cpp
        tabbox/clientmodel.cpp
        tabbox/desktopitemdelegate.cpp
        tabbox/desktopmodel.cpp
        tabbox/itemlayoutconfig.cpp
        tabbox/tabboxconfig.cpp
        tabbox/tabboxhandler.cpp
        tabbox/tabboxview.cpp
  )
endif(KWIN_BUILD_TABBOX)

qt4_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.KWin.xml workspace.h KWin::Workspace )

qt4_add_dbus_interface( kwin_KDEINIT_SRCS
  ${KDEBASE_WORKSPACE_SOURCE_DIR}/ksmserver/org.kde.KSMServerInterface.xml ksmserver_interface)

qt4_add_resources( kwin_KDEINIT_SRCS resources.qrc )

kde4_add_kdeinit_executable( kwin ${kwin_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} ${QT_QTXML_LIBRARY} ${QT_QTSCRIPT_LIBRARY} kephal kworkspace kdecorations kwineffects ${X11_LIBRARIES})

if(OPENGL_FOUND AND NOT KWIN_HAVE_OPENGLES_COMPOSITING)
  add_subdirectory(opengltest)
  target_link_libraries(kdeinit_kwin ${OPENGL_gl_LIBRARY})
  # -ldl used by OpenGL code
  find_library(DL_LIBRARY dl)
  if (DL_LIBRARY)
    target_link_libraries(kdeinit_kwin ${DL_LIBRARY})
  endif(DL_LIBRARY)
  # must be after opengl, to be initialized first by the linker
  target_link_libraries(kdeinit_kwin kwinnvidiahack)
endif(OPENGL_FOUND AND NOT KWIN_HAVE_OPENGLES_COMPOSITING)

if(KWIN_HAVE_OPENGLES_COMPOSITING)
  target_link_libraries(kdeinit_kwin ${OPENGLES_LIBRARIES} ${OPENGLES_EGL_LIBRARIES})
endif(KWIN_HAVE_OPENGLES_COMPOSITING)

if (X11_Xrandr_FOUND)
  target_link_libraries(kdeinit_kwin ${X11_Xrandr_LIB})
endif (X11_Xrandr_FOUND)
if (X11_Xcomposite_FOUND)
  target_link_libraries(kdeinit_kwin ${X11_Xcomposite_LIB})
endif (X11_Xcomposite_FOUND)
if (X11_Xdamage_FOUND)
  target_link_libraries(kdeinit_kwin ${X11_Xdamage_LIB})
endif (X11_Xdamage_FOUND)
if (X11_Xrender_FOUND)
  target_link_libraries(kdeinit_kwin ${X11_Xrender_LIB})
endif (X11_Xrender_FOUND)
if (X11_Xfixes_FOUND)
  target_link_libraries(kdeinit_kwin ${X11_Xfixes_LIB})
endif (X11_Xfixes_FOUND)

install(TARGETS kdeinit_kwin ${INSTALL_TARGETS_DEFAULT_ARGS} )
install(TARGETS kwin         ${INSTALL_TARGETS_DEFAULT_ARGS} )


########### next target ###############

set( kwinnvidiahack_LIB_SRCS 
    nvidiahack.cpp )


kde4_add_library(kwinnvidiahack SHARED ${kwinnvidiahack_LIB_SRCS})

set_target_properties(kwinnvidiahack PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION}  )
install(TARGETS kwinnvidiahack ${INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)

########### install files ###############

install( FILES kwin.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES kwin.notifyrc  DESTINATION  ${DATA_INSTALL_DIR}/kwin )
install( FILES org.kde.KWin.xml DESTINATION ${DBUS_INTERFACES_INSTALL_DIR} )
install( FILES tabbox/DefaultTabBoxLayouts.xml DESTINATION ${DATA_INSTALL_DIR}/kwin )

kde4_install_icons( ${ICON_INSTALL_DIR} )
