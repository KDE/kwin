add_subdirectory( lib ) 

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/lib
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
    )

add_subdirectory( killer )
add_subdirectory( kcmkwin )
add_subdirectory( clients )
add_subdirectory( effects )
add_subdirectory( data )

configure_file(config-kwin.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kwin.h )
include_directories(${CMAKE_CURRENT_BINARY_DIR})


########### next target ###############

set(kwin_KDEINIT_SRCS
   workspace.cpp 
   client.cpp 
   placement.cpp 
   atoms.cpp 
   utils.cpp 
   layers.cpp 
   main.cpp 
   popupinfo.cpp 
   tabbox.cpp 
   options.cpp 
   plugins.cpp 
   events.cpp 
   killwindow.cpp 
   geometrytip.cpp 
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
   deleted.cpp
   effects.cpp
   compositingprefs.cpp
   )

qt4_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.KWin.xml workspace.h KWin::Workspace )



kde4_add_kdeinit_executable( kwin ${kwin_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin ${KDE4_KDEUI_LIBS} kdecorations kwineffects ${X11_LIBRARIES})
if(OPENGL_FOUND)
  target_link_libraries(kdeinit_kwin ${OPENGL_gl_LIBRARY})
  # -ldl used by OpenGL code
  find_library(DL_LIBRARY dl)
  if (DL_LIBRARY)
    target_link_libraries(kdeinit_kwin ${DL_LIBRARY})
  endif(DL_LIBRARY)
  # must be after opengl, to be initialized first by the linker
  target_link_libraries(kdeinit_kwin kwinnvidiahack)
endif(OPENGL_FOUND)
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

install(TARGETS kdeinit_kwin  DESTINATION ${LIB_INSTALL_DIR} )

target_link_libraries( kwin kdeinit_kwin )
install(TARGETS kwin DESTINATION ${BIN_INSTALL_DIR})


########### next target ###############

set( kwinnvidiahack_LIB_SRCS 
    nvidiahack.cpp )


kde4_add_library(kwinnvidiahack SHARED ${kwinnvidiahack_LIB_SRCS})

set_target_properties(kwinnvidiahack PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION}  )
install(TARGETS kwinnvidiahack  DESTINATION ${LIB_INSTALL_DIR} )

########### install files ###############

install( FILES kwin.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES kwin.notifyrc  DESTINATION  ${DATA_INSTALL_DIR}/kwin )
install( FILES org.kde.KWin.xml DESTINATION ${DBUS_INTERFACES_INSTALL_DIR} )

kde4_install_icons( ${ICON_INSTALL_DIR} )

# cmake apparently doesn't allow nested ()'s in if(), so write as multiple if()'s
if( X11_Xcomposite_FOUND AND X11_Xdamage_FOUND )
  if( OPENGL_FOUND )
    SET(KWIN_COMPOSITE_ENABLED TRUE)
  endif( OPENGL_FOUND )
  if( X11_Xrender_FOUND AND X11_Xfixes_FOUND )
    SET(KWIN_COMPOSITE_ENABLED TRUE)
  endif( X11_Xrender_FOUND AND X11_Xfixes_FOUND )
endif( X11_Xcomposite_FOUND AND X11_Xdamage_FOUND )
macro_log_feature(KWIN_COMPOSITE_ENABLED "Compositing support" "XComposite and XDamage extensions and OpenGL or XRender with XFixes are required for KWin compositing support" "http://www.x.org/")
