INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwinb2_config.la

libkwinb2_config_la_SOURCES = config.cpp
libkwinb2_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)
libkwinb2_config_la_LIBADD = $(LIB_KDEUI)

METASOURCES = AUTO
noinst_HEADERS = config.h

lnkdir = $(kde_datadir)/kwin/

messages:
	$(XGETTEXT) *.cpp -o $(podir)/libkwinb2_config.pot

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
