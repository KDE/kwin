
INCLUDES = $(all_includes)
LDFLAGS = $(all_libraries) $(KDE_RPATH)

SUBDIRS = pics

bin_PROGRAMS = kwin

kwin_SOURCES = atoms.cpp beclient.cpp client.cpp main.cpp stdclient.cpp workspace.cpp tabbox.cpp options.cpp systemclient.cpp nextclient.cpp

kwin_LDADD = $(LIB_KDECORE) $(LIB_KDEUI)

KDE_ICON = kwin

METASOURCES = AUTO

messages:
	$(XGETTEXT) -C -ktranslate -kaliasLocal -ki18n -x $(includedir)/kde.pot *.h *.cpp && mv messages.po $(podir)/kwin.pot

