INCLUDES = -I$(top_srcdir)/kwin/default $(all_includes)

SUBDIRS = . pics clients

bin_PROGRAMS = kwin
lib_LTLIBRARIES = kwin.la

# workspace.cpp has to be first in order not to break --enable-final
kwin_la_SOURCES = workspace.cpp atoms.cpp  client.cpp main.cpp \
	tabbox.cpp options.cpp plugins.cpp events.cpp KWinInterface.skel \
	killwindow.cpp kwinbutton.cpp
kwin_la_LIBADD = $(LIB_KDEUI) $(LIBXINERAMA)
kwin_la_LDFLAGS = $(all_libraries) -module -avoid-version

include_HEADERS = KWinInterface.h

kwinincludedir = $(includedir)/kwin
kwininclude_HEADERS = options.h client.h workspace.h kwinbutton.h

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

