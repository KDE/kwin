INCLUDES = $(all_includes) -I$(top_srcdir)/kwin/default

SUBDIRS = default . pics clients

bin_PROGRAMS = kwin
lib_LTLIBRARIES = kwin.la

kwin_la_SOURCES = atoms.cpp  client.cpp main.cpp workspace.cpp \
	tabbox.cpp options.cpp plugins.cpp events.cpp KWinInterface.skel \
	killwindow.cpp 
kwin_la_LIBADD = $(LIB_KDEUI) $(top_builddir)/kwin/default/libkwindefault.la
kwin_la_LDFLAGS = $(all_libraries) -module -avoid-version

include_HEADERS = KWinInterface.h

kwinincludedir = $(includedir)/kwin
kwininclude_HEADERS = options.h client.h workspace.h

kwin_SOURCES = kwin_main.cpp
kwin_LDADD = kwin.la
kwin_LDFLAGS = $(all_libraries) $(KDE_RPATH)

KDE_ICON = kwin

METASOURCES = AUTO

messages:
	$(XGETTEXT) -kaliasLocal *.h *.cpp -o $(podir)/kwin.pot

datadir = $(kde_datadir)/kwin

data_DATA= eventsrc

sounddir= $(kde_sounddir)
sound_DATA = pop.wav

