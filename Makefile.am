
INCLUDES = $(all_includes)
LDFLAGS = $(all_libraries) $(KDE_RPATH) -export-dynamic

SUBDIRS = pics clients

bin_PROGRAMS = kwin

kwin_SOURCES = atoms.cpp  client.cpp main.cpp stdclient.cpp workspace.cpp tabbox.cpp options.cpp plugins.cpp

kwin_LDADD = $(LIB_KDEUI)

KDE_ICON = kwin

METASOURCES = AUTO

messages:
	$(XGETTEXT) -C -ktranslate -kaliasLocal -kI18N_NOOP -ki18n -x $(includedir)/kde.pot *.h *.cpp && mv messages.po $(podir)/kwin.pot

