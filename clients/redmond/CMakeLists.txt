
include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )



########### next target ###############

set(kwin3_redmond_PART_SRCS redmond.cpp )

kde4_automoc(kwin3_redmond ${kwin3_redmond_PART_SRCS})

kde4_add_plugin(kwin3_redmond ${kwin3_redmond_PART_SRCS})



target_link_libraries(kwin3_redmond  ${KDE4_KDECORE_LIBS} kdefx kdecorations )

install(TARGETS kwin3_redmond  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES redmond.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )




#original Makefile.am contents follow:

#
#INCLUDES = -I$(srcdir)/../../lib $(all_includes)
#
#kde_module_LTLIBRARIES = kwin3_redmond.la
#
#kwin3_redmond_la_SOURCES = redmond.cpp
#kwin3_redmond_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_redmond_la_LIBADD = $(LIB_KDECORE) -lkdefx ../../lib/libkdecorations.la
#
#METASOURCES = AUTO
#noinst_HEADERS = redmond.h
#
#lnkdir = $(kde_datadir)/kwin
#lnk_DATA = redmond.desktop
#
#EXTRA_DIST = $(lnk_DATA)
#
