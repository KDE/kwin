
INCLUDES = $(all_includes)

SUBDIRS = pics clients

bin_PROGRAMS = kwin
lib_LTLIBRARIES = kwin.la

kwin_la_SOURCES = atoms.cpp  client.cpp main.cpp stdclient.cpp workspace.cpp tabbox.cpp options.cpp plugins.cpp
kwin_la_LIBADD = $(LIB_KDEUI)
kwin_la_LDFLAGS = $(all_libraries) -module -avoid-version

kwin_SOURCES = dummy.cpp
kwin_LDADD = kwin.la
kwin_LDFLAGS = $(all_libraries) $(KDE_RPATH) -export-dynamic -rdynamic

KDE_ICON = kwin

METASOURCES = AUTO

messages:
	$(XGETTEXT) -kaliasLocal *.h *.cpp -o $(podir)/kwin.pot

datadir = $(kde_datadir)/kwin

data_DATA= eventsrc

dummy.cpp:
	echo > dummy.cpp

