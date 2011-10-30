########### configure tests ###############

OPTION(KWIN_BUILD_DECORATIONS "Enable building of KWin decorations." ON)
OPTION(KWIN_BUILD_KCMS "Enable building of KWin configuration modules." ON)
OPTION(KWIN_MOBILE_EFFECTS "Only build effects relevant for mobile devices" OFF)
OPTION(KWIN_BUILD_TABBOX "Enable building of KWin Tabbox functionality" ON)
OPTION(KWIN_BUILD_TILING "Enable building of KWin Tiling functionality" ON)
OPTION(KWIN_BUILD_DESKTOPCHANGEOSD "Enable building of KWin DesktopChangeOSD funtionality" ON)
OPTION(KWIN_BUILD_SCREENEDGES "Enable building of KWin with screen edge support" ON)
OPTION(KWIN_BUILD_SCRIPTING "Enable building of KWin with scripting support" ON)
OPTION(KWIN_BUILD_XRENDER_COMPOSITING "Enable building of KWin with XRender Compositing support" ON)
if(${KDE_PLATFORM_PROFILE} STREQUAL "Desktop")
   OPTION(KWIN_PLASMA_ACTIVE "Enable building KWin for Plasma Active." OFF)
else(${KDE_PLATFORM_PROFILE} STREQUAL "Desktop")
   OPTION(KWIN_PLASMA_ACTIVE "Enable building KWin for Plasma Active." On)
endif(${KDE_PLATFORM_PROFILE} STREQUAL "Desktop")

if(KWIN_PLASMA_ACTIVE)
    set(KWIN_BUILD_DECORATIONS OFF)
    set(KWIN_BUILD_KCMS OFF)
    set(KWIN_BUILD_TABBOX OFF)
    set(KWIN_BUILD_TILING OFF)
    set(KWIN_BUILD_DESKTOPCHANGEOSD OFF)
    set(KWIN_BUILD_SCREENEDGES OFF)
    set(KWIN_BUILD_SCRIPTING OFF)
    set(KWIN_BUILD_XRENDER_COMPOSITING OFF)
    set(KWIN_MOBILE_EFFECTS ON)
    set(KWIN_BUILD_WITH_OPENGLES ON)
endif(KWIN_PLASMA_ACTIVE)

# KWIN_HAVE_XRENDER_COMPOSITING - whether XRender-based compositing support is available: may be disabled
if( KWIN_BUILD_XRENDER_COMPOSITING )
    set( KWIN_HAVE_XRENDER_COMPOSITING 1 )
endif( KWIN_BUILD_XRENDER_COMPOSITING )

if(OPENGL_FOUND)
  include_directories(${OPENGL_INCLUDE_DIR})
endif(OPENGL_FOUND)

if(OPENGLES_FOUND)
  include_directories(${OPENGLES_INCLUDE_DIR})
endif(OPENGLES_FOUND)

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

add_subdirectory( effects )

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
   overlaywindow.cpp
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
   )

if(KWIN_BUILD_SCRIPTING)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
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
    )
endif(KWIN_BUILD_SCRIPTING)

if(KWIN_BUILD_TABBOX)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        tabbox/tabbox.cpp
        tabbox/clientitemdelegate.cpp
        tabbox/clientmodel.cpp
        tabbox/declarative.cpp
        tabbox/desktopitemdelegate.cpp
        tabbox/desktopmodel.cpp
        tabbox/itemlayoutconfig.cpp
        tabbox/tabboxconfig.cpp
        tabbox/tabboxhandler.cpp
        tabbox/tabboxview.cpp
  )
endif(KWIN_BUILD_TABBOX)

if(KWIN_BUILD_TILING)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        tiling/tile.cpp
        tiling/tiling.cpp
        tiling/tilinglayout.cpp
        tilinglayoutfactory.cpp

        # tiling layouts
        # spiral
        tilinglayouts/spiral/spiral.cpp

        # columns
        tilinglayouts/columns/columns.cpp

        # floating
        tilinglayouts/floating/floating.cpp
   )
endif(KWIN_BUILD_TILING)

if(KWIN_BUILD_DESKTOPCHANGEOSD)
    set(kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        desktopchangeosd.cpp
    )
endif(KWIN_BUILD_DESKTOPCHANGEOSD)

if(KWIN_BUILD_SCREENEDGES)
    set(
        kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
        screenedge.cpp
    )
endif(KWIN_BUILD_SCREENEDGES)

qt4_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.KWin.xml workspace.h KWin::Workspace )

qt4_add_dbus_interface( kwin_KDEINIT_SRCS
  ${KDEBASE_WORKSPACE_SOURCE_DIR}/ksmserver/org.kde.KSMServerInterface.xml ksmserver_interface)

qt4_add_resources( kwin_KDEINIT_SRCS resources.qrc )

set(kwinLibs ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} kephal kworkspace kdecorations kwineffects ${X11_LIBRARIES} ${X11_Xrandr_LIB} ${X11_Xcomposite_LIB} ${X11_Xdamage_LIB} ${X11_Xrender_LIB} ${X11_Xfixes_LIB})

if(KWIN_BUILD_SCRIPTING)
    set(kwinLibs ${kwinLibs} ${QT_QTSCRIPT_LIBRARY})
endif(KWIN_BUILD_SCRIPTING)

if(KWIN_BUILD_TABBOX)
    set(kwinLibs ${kwinLibs} ${QT_QTXML_LIBRARY} ${QT_QTDECLARATIVE_LIBRARY} kdeclarative)
endif(KWIN_BUILD_TABBOX)

kde4_add_kdeinit_executable( kwin ${kwin_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin ${kwinLibs})

if(OPENGL_FOUND)
  set_target_properties(kdeinit_kwin PROPERTIES COMPILE_FLAGS -DKWIN_HAVE_OPENGL)
  add_subdirectory(opengltest)
  target_link_libraries(kdeinit_kwin kwinglutils ${OPENGL_gl_LIBRARY})
  # -ldl used by OpenGL code
  find_library(DL_LIBRARY dl)
  if (DL_LIBRARY)
    target_link_libraries(kdeinit_kwin ${DL_LIBRARY})
  endif(DL_LIBRARY)
  # must be after opengl, to be initialized first by the linker
  target_link_libraries(kdeinit_kwin kwinnvidiahack)
elseif(OPENGLES_FOUND)
  target_link_libraries(kdeinit_kwin ${kwinLibs} kwinglesutils ${OPENGLES_LIBRARIES})
  set_target_properties(kdeinit_kwin PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGL -DKWIN_HAVE_OPENGLES")
endif(OPENGL_FOUND)

install(TARGETS kdeinit_kwin ${INSTALL_TARGETS_DEFAULT_ARGS} )
install(TARGETS kwin         ${INSTALL_TARGETS_DEFAULT_ARGS} )

if(OPENGLES_FOUND)
  kde4_add_kdeinit_executable( kwin_gles ${kwin_KDEINIT_SRCS})
  target_link_libraries(kdeinit_kwin_gles ${kwinLibs} kwinglesutils ${OPENGLES_LIBRARIES})
  set_target_properties(kdeinit_kwin_gles PROPERTIES COMPILE_FLAGS "-DKWIN_HAVE_OPENGL -DKWIN_HAVE_OPENGLES")
  install(TARGETS kdeinit_kwin_gles ${INSTALL_TARGETS_DEFAULT_ARGS} )
  install(TARGETS kwin_gles         ${INSTALL_TARGETS_DEFAULT_ARGS} )
endif(OPENGLES_FOUND)

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
install( FILES
    tabbox/qml/informative.qml
    tabbox/qml/big_icons.qml
    tabbox/qml/compact.qml
    tabbox/qml/small_icons.qml
    tabbox/qml/tabbox.qml
    tabbox/qml/text.qml
    tabbox/qml/IconTabBox.qml
    DESTINATION ${DATA_INSTALL_DIR}/kwin/tabbox )

kde4_install_icons( ${ICON_INSTALL_DIR} )
