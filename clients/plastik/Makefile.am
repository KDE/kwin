AUTOMAKE_OPTIONS = foreign

SUBDIRS = config

KDE_CXXFLAGS = -DQT_PLUGIN

INCLUDES = -I$(srcdir)/../../lib $(all_includes)

kwindir = $(kde_datadir)/kwin/
kwin_DATA = plastik.desktop

kde_module_LTLIBRARIES = kwin3_plastik.la
kwin3_plastik_la_SOURCES = plastik.cpp plastikclient.cpp plastikbutton.cpp misc.cpp
kwin3_plastik_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin3_plastik_la_LIBADD = $(LIB_KDEUI) ../../lib/libkdecorations.la
kwin3_plastik_la_METASOURCES = AUTO

DISTCLEANFILES = $(kwin3_plastik_la_METASOURCES)

