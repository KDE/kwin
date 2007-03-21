
project(kwin)

add_subdirectory( lib ) 
add_subdirectory( killer ) 
add_subdirectory( kcmkwin ) 
add_subdirectory( pics ) 
add_subdirectory( clients ) 
add_subdirectory( data ) 

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/effects
    )

include(UsePkgConfig)
PKGCONFIG(libcaptury CAPTURY_INCLUDES CAPTURY_LINK_DIR CAPTURY_LDFLAGS CAPTURY_CFLAGS)
if( CAPTURY_LDFLAGS )
    SET( CAPTURY_FOUND TRUE )
endif( CAPTURY_LDFLAGS )
macro_bool_to_01( CAPTURY_FOUND HAVE_CAPTURY )

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
   glutils.cpp
   glutils_funcs.cpp
   deleted.cpp
   effects.cpp
   effects/fade.cpp
   effects/maketransparent.cpp
   effects/scalein.cpp
   effects/shakymove.cpp
   effects/shiftworkspaceup.cpp
   effects/howto.cpp
   effects/dialogparent.cpp
   effects/showfps.cpp
   effects/zoom.cpp
   effects/presentwindows.cpp
   effects/minimizeanimation.cpp
   effects/desktopchangeslide.cpp
   effects/boxswitch.cpp
   effects/fallapart.cpp
   effects/drunken.cpp
   effects/flame.cpp
   effects/test_input.cpp
   effects/test_thumbnail.cpp
   )

if( CAPTURY_FOUND )
   set(kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
   effects/videorecord.cpp
   )
    include_directories(${CAPTURY_INCLUDES})
endif( CAPTURY_FOUND )

if(HAVE_OPENGL)
   set(kwin_KDEINIT_SRCS ${kwin_KDEINIT_SRCS}
   effects/wavywindows.cpp
   effects/explosioneffect.cpp
   )
endif(HAVE_OPENGL)

qt4_add_dbus_adaptor( kwin_KDEINIT_SRCS org.kde.KWin.xml workspace.h KWinInternal::Workspace )

kde4_automoc(kwin ${kwin_KDEINIT_SRCS})


kde4_add_kdeinit_executable( kwin ${kwin_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin  ${KDE4_KDEUI_LIBS} kdecorations ${X11_LIBRARIES} ${QT_QT3SUPPORT_LIBRARY} )
if(OPENGL_FOUND)
  target_link_libraries(kdeinit_kwin ${OPENGL_gl_LIBRARY})
  # -ldl used by OpenGL code
  target_link_libraries(kdeinit_kwin -ldl)
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
if( HAVE_CAPTURY )
  target_link_libraries(kdeinit_kwin ${CAPTURY_LDFLAGS})
endif( HAVE_CAPTURY )


install(TARGETS kdeinit_kwin  DESTINATION ${LIB_INSTALL_DIR} )

target_link_libraries( kwin kdeinit_kwin )
install(TARGETS kwin DESTINATION bin)

########### install files ###############

install( FILES kwin.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES kwin.notifyrc  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

install( FILES effects/data/explosion.frag effects/data/explosion.vert effects/data/explosion-start.png effects/data/explosion-end.png   DESTINATION  ${DATA_INSTALL_DIR}/kwin )

kde4_install_icons( ${ICON_INSTALL_DIR} crystalsvg )




#original Makefile.am contents follow:

#INCLUDES = -I$(srcdir)/lib $(all_includes)
#
#SUBDIRS = lib . killer kcmkwin pics clients oldheaders data
#
#bin_PROGRAMS = 
#lib_LTLIBRARIES =
#kdeinit_LTLIBRARIES = kwin.la
#
#kwin_la_SOURCES = workspace.cpp client.cpp placement.cpp atoms.cpp \
#	utils.cpp layers.cpp main.cpp popupinfo.cpp tabbox.cpp \
#	 options.cpp plugins.cpp events.cpp KWinInterface.skel \
#	killwindow.cpp geometrytip.cpp sm.cpp group.cpp bridge.cpp \
#	manage.cpp notifications.cpp activation.cpp useractions.cpp \
#	geometry.cpp rules.cpp
#
#kwin_la_LIBADD = $(LIB_KDEUI) lib/libkdecorations.la
#kwin_la_LDFLAGS = $(all_libraries) -module -avoid-version
#
#include_HEADERS = KWinInterface.h
#
#KDE_ICON = kwin
#
#METASOURCES = AUTO
#
#messages: rc.cpp
#	$(XGETTEXT) *.h *.cpp killer/*.cpp lib/*.cpp -o $(podir)/kwin.pot
#
#kwin_datadir = $(kde_datadir)/kwin
#
#kwin_data_DATA= eventsrc
#
#kde_kcfg_DATA = kwin.kcfg
#
#include ../../admin/Doxyfile.am
