
INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = kwin_laptop.la

kwin_laptop_la_SOURCES = laptopclient.cpp
kwin_laptop_la_LIBADD = ../../kwin.la
kwin_laptop_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES = AUTO
noinst_HEADERS = laptopclient.h

lnkdir = $(kde_datadir)/kwin/
lnk_DATA = laptop.desktop

EXTRA_DIST = $(lnk_DATA)


###KMAKE-start (don't edit or delete this block)

###KMAKE-end
