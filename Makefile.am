INCLUDES = $(all_includes)

SUBDIRS = . kcmkwin pics clients

bin_PROGRAMS = 
lib_LTLIBRARIES = 
kdeinit_LTLIBRARIES = kwin.la

# workspace.cpp has to be first in order not to break --enable-final
kwin_la_SOURCES = workspace.cpp client.cpp placement.cpp atoms.cpp  main.cpp \
	popupinfo.cpp tabbox.cpp options.cpp plugins.cpp events.cpp KWinInterface.skel \
	killwindow.cpp kwinbutton.cpp geometrytip.cpp
kwin_la_LIBADD = $(LIB_KDEUI)
kwin_la_LDFLAGS = $(all_libraries) -module -avoid-version

include_HEADERS = KWinInterface.h

kwinincludedir = $(includedir)/kwin
kwininclude_HEADERS = options.h client.h workspace.h kwinbutton.h

KDE_ICON = kwin

METASOURCES = AUTO

messages:
	$(XGETTEXT) -kaliasLocal *.h *.cpp -o $(podir)/kwin.pot

kwin_datadir = $(kde_datadir)/kwin

kwin_data_DATA= eventsrc

sounddir= $(kde_sounddir)
sound_DATA = pop.wav

update_DATA = kwin.upd pluginlibFix.pl
updatedir = $(kde_datadir)/kconf_update

