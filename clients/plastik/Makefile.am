AUTOMAKE_OPTIONS = foreign

SUBDIRS = config

KDE_CXXFLAGS = -DQT_PLUGIN

INCLUDES = $(all_includes) -I$(kde_includes)/kwin

kwindir = $(kde_datadir)/kwin/
kwin_DATA = plastik.desktop

noinst_HEADERS = plastik.h plastikclient.h plastikbutton.h misc.h shadow.h xpm/empty.xpm xpm/close.xpm xpm/help.xpm xpm/maximize.xpm xpm/restore.xpm xpm/minimize.xpm xpm/sticky.xpm xpm/unsticky.xpm

kde_module_LTLIBRARIES = kwin_plastik.la
kwin_plastik_la_SOURCES = plastik.cpp plastikclient.cpp plastikbutton.cpp misc.cpp shadow.cpp
kwin_plastik_la_LDFLAGS = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)
kwin_plastik_la_METASOURCES = AUTO

DISTCLEANFILES = $(kwin_plastik_la_METASOURCES)
