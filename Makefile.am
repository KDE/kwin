INCLUDES = -I$(srcdir)/lib $(all_includes)

SUBDIRS = lib . killer kcmkwin pics clients oldheaders

bin_PROGRAMS = 
lib_LTLIBRARIES =
kdeinit_LTLIBRARIES = kwin.la

kwin_la_SOURCES = workspace.cpp client.cpp placement.cpp atoms.cpp \
	utils.cpp layers.cpp main.cpp popupinfo.cpp tabbox.cpp \
	 options.cpp plugins.cpp events.cpp KWinInterface.skel \
	killwindow.cpp geometrytip.cpp sm.cpp group.cpp bridge.cpp \
	manage.cpp notifications.cpp activation.cpp useractions.cpp \
	geometry.cpp

kwin_la_LIBADD = $(LIB_KDEUI) lib/libkdecorations.la
kwin_la_LDFLAGS = $(all_libraries) -module -avoid-version

include_HEADERS = KWinInterface.h

KDE_ICON = kwin

METASOURCES = AUTO

messages:
	$(XGETTEXT) -kaliasLocal *.h *.cpp killer/*.cpp lib/*.cpp -o $(podir)/kwin.pot

kwin_datadir = $(kde_datadir)/kwin

kwin_data_DATA= eventsrc

kde_kcfg_DATA = kwin.kcfg

sounddir= $(kde_sounddir)
sound_DATA = pop.wav

update_DATA = kwin.upd kwinsticky.upd kwiniconify.upd kwin3_plugin.upd kwin_focus1.upd
update_SCRIPTS = pluginlibFix.pl kwin3_plugin.pl kwin_focus1.sh

updatedir = $(kde_datadir)/kconf_update

include ../admin/Doxyfile.am
