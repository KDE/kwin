INCLUDES                     = $(all_includes)

kde_module_LTLIBRARIES       = kwin_plastik_config.la

kwin_plastik_config_la_SOURCES = config.cpp configdialog.ui
kwin_plastik_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin_plastik_config_la_LIBADD  = $(LIB_KDEUI)

METASOURCES                  = AUTO
noinst_HEADERS               = config.h
DISTCLEANFILES               = $(METASOURCES)

lnkdir                       = $(kde_datadir)/kwin

