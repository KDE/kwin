
project(kwin)

add_subdirectory( lib ) 
add_subdirectory( killer ) 
add_subdirectory( kcmkwin ) 
add_subdirectory( pics ) 
add_subdirectory( clients ) 
add_subdirectory( data ) 

if(X11_kompmgr_FOUND)
   add_subdirectory(kompmgr)
endif(X11_kompmgr_FOUND)

include_directories( ${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_CURRENT_SOURCE_DIR}/lib )



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
   rules.cpp )

kde4_automoc(${kwin_KDEINIT_SRCS})


kde4_add_kdeinit_executable( kwin ${kwin_KDEINIT_SRCS})

target_link_libraries(kdeinit_kwin  ${KDE4_KDEUI_LIBS} kdecorations ${X11_LIBRARIES} ${QT_QT3SUPPORT_LIBRARY} )

install(TARGETS kdeinit_kwin  DESTINATION ${LIB_INSTALL_DIR} )

target_link_libraries( kwin kdeinit_kwin )
install(TARGETS kwin DESTINATION ${BIN_INSTALL_DIR})

########### install files ###############

install( FILES kwin.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES eventsrc  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

kde4_install_icons( ${ICON_INSTALL_DIR} crystalsvg )




#original Makefile.am contents follow:

#INCLUDES = -I$(srcdir)/lib $(all_includes)
#
#if include_kompmgr
#KOMPMGR=kompmgr
#endif
#
#SUBDIRS = lib . killer kcmkwin pics clients oldheaders data $(KOMPMGR)
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
