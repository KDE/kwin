
INCLUDES = $(all_includes) 

kde_module_LTLIBRARIES = kwin_kstep.la

kwin_kstep_la_SOURCES = nextclient.cpp
kwin_kstep_la_LIBADD = ../../kwin.la
kwin_kstep_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = nextclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = kstep.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
