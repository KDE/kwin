INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkwinglow_config.la

libkwinglow_config_la_SOURCES = glowconfigdialog.cpp
libkwinglow_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)
libkwinglow_config_la_LIBADD = $(LIB_KDEUI)

METASOURCES = AUTO
noinst_HEADERS = glowconfigdialog.h

lnkdir = $(kde_datadir)/kwin/

messages:
	$(XGETTEXT) *.cpp -o $(podir)/libkwinglow_config.pot

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
