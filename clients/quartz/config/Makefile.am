INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwinquartz_config.la

libkwinquartz_config_la_SOURCES = config.cpp
libkwinquartz_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)
libkwinquartz_config_la_LIBADD = $(LIB_KDEUI)

METASOURCES = AUTO
noinst_HEADERS = config.h

lnkdir = $(kde_datadir)/kwin/

messages:
	$(XGETTEXT) *.cpp -o $(podir)/libkwinquartz_config.pot

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
