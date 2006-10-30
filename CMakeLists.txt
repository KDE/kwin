
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



########### next target ###############

set(kwin_KDEINIT_SRCS
   kwinadaptor.cpp
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
   effects.cpp
   effects/fadein.cpp
   effects/maketransparent.cpp
   effects/scalein.cpp
   effects/shakymove.cpp
   effects/shiftworkspaceup.cpp
   effects/howto.cpp
   )

kde4_automoc(kwin ${kwin_KDEINIT_SRCS})


kde4_add_kdeinit_executable( kwin ${kwin_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin  ${KDE4_KDEUI_LIBS} kdecorations ${X11_LIBRARIES} ${QT_QT3SUPPORT_LIBRARY} )
target_link_libraries(kdeinit_kwin -lGL)
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
install(TARGETS kwin DESTINATION bin)

########### install files ###############

install( FILES kwin.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES eventsrc  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

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
