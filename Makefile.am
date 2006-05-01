INCLUDES = -I$(srcdir)/lib $(all_includes)

if include_kompmgr
KOMPMGR=kompmgr
endif

SUBDIRS = lib . killer kcmkwin pics clients oldheaders data $(KOMPMGR)

bin_PROGRAMS = 
lib_LTLIBRARIES =
kdeinit_LTLIBRARIES = kwin.la

kwin_la_SOURCES = workspace.cpp client.cpp placement.cpp atoms.cpp \
	utils.cpp layers.cpp main.cpp popupinfo.cpp tabbox.cpp \
	 options.cpp plugins.cpp events.cpp KWinInterface.skel \
	killwindow.cpp geometrytip.cpp sm.cpp group.cpp bridge.cpp \
	manage.cpp notifications.cpp activation.cpp useractions.cpp \
	geometry.cpp rules.cpp

kwin_la_LIBADD = $(LIB_KDEUI) lib/libkdecorations.la
kwin_la_LDFLAGS = $(all_libraries) -module -avoid-version

include_HEADERS = KWinInterface.h

KDE_ICON = kwin

METASOURCES = AUTO


kwin_datadir = $(kde_datadir)/kwin

kwin_data_DATA= eventsrc

kde_kcfg_DATA = kwin.kcfg

include ../../admin/Doxyfile.am
