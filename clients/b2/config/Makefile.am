INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = kwin_b2_config.la

kwin_b2_config_la_SOURCES = config.cpp
kwin_b2_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin_b2_config_la_LIBADD = $(LIB_KDEUI)

METASOURCES = AUTO
noinst_HEADERS = config.h

lnkdir = $(kde_datadir)/kwin/

###KMAKE-start (don't edit or delete this block)

###KMAKE-end
