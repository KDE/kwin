
INCLUDES = -I$(srcdir)/../../lib $(all_includes)

kde_module_LTLIBRARIES = kwin3_redmond.la

kwin3_redmond_la_SOURCES = redmond.cpp
kwin3_redmond_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin3_redmond_la_LIBADD = $(LIB_KDECORE) ../../lib/libkdecorations.la

METASOURCES = AUTO
noinst_HEADERS = redmond.h
