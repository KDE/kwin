INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwinicewm_config.la

libkwinicewm_config_la_SOURCES = config.cpp
libkwinicewm_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)
libkwinicewm_config_la_LIBADD = $(LIB_KDEUI) $(LIB_KFILE)

METASOURCES = AUTO
noinst_HEADERS = config.h

lnkdir = $(kde_datadir)/kwin/

messages:
	$(XGETTEXT) *.cpp -o $(podir)/libkwinicewm_config.pot

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
