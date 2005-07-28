
INCLUDES = -I$(srcdir)/../../lib $(all_includes)

SUBDIRS = . config

kde_module_LTLIBRARIES = kwin3_default.la

kwin3_default_la_SOURCES = kdedefault.cpp
kwin3_default_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin3_default_la_LIBADD  = $(LIB_KDECORE) -lkdefx ../../lib/libkdecorations.la

METASOURCES = AUTO
noinst_HEADERS = kdedefault.h

