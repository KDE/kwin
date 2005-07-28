
INCLUDES = -I$(srcdir)/../../lib $(all_includes)

kde_module_LTLIBRARIES = kwin3_laptop.la

kwin3_laptop_la_SOURCES = laptopclient.cpp
kwin3_laptop_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module 
kwin3_laptop_la_LIBADD  = ../../lib/libkdecorations.la -lkdefx

METASOURCES = AUTO
noinst_HEADERS = laptopclient.h

lnkdir = $(kde_datadir)/kwin
lnk_DATA = laptop.desktop

EXTRA_DIST = $(lnk_DATA)

