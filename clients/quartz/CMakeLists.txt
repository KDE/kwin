
add_subdirectory( config ) 




########### next target ###############

set(kwin3_quartz_PART_SRCS quartz.cpp )

kde4_automoc(kwin3_quartz ${kwin3_quartz_PART_SRCS})

kde4_add_plugin(kwin3_quartz ${kwin3_quartz_PART_SRCS})



target_link_libraries(kwin3_quartz  ${KDE4_KDEUI_LIBS} kdecorations kdefx)

install(TARGETS kwin3_quartz  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES quartz.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )




#original Makefile.am contents follow:

#
#INCLUDES = -I$(srcdir)/../../lib $(all_includes)
#
#SUBDIRS = . config
#
#kde_module_LTLIBRARIES = kwin3_quartz.la
#
#kwin3_quartz_la_SOURCES = quartz.cpp
#kwin3_quartz_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
#kwin3_quartz_la_LIBADD  = $(LIB_KDEUI)  ../../lib/libkdecorations.la
#
#METASOURCES = AUTO
#noinst_HEADERS = quartz.h
#
#lnkdir = $(kde_datadir)/kwin/
#lnk_DATA = quartz.desktop
#
#EXTRA_DIST = $(lnk_DATA)
#
#
####KMAKE-start (don't edit or delete this block)
#
####KMAKE-end
