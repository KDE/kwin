INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwindefault_config.la

libkwindefault_config_la_SOURCES = config.cpp
libkwindefault_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)
libkwindefault_config_la_LIBADD = $(LIB_KDEUI)

METASOURCES = AUTO
noinst_HEADERS = config.h

lnkdir = $(kde_datadir)/kwin/

messages:
	$(XGETTEXT) *.cpp -o $(podir)/libkwindefault_config.pot

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
