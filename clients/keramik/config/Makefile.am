INCLUDES                       = $(all_includes)

kde_module_LTLIBRARIES         = kwin_keramik_config.la

kwin_keramik_config_la_SOURCES = config.cpp keramikconfig.ui
kwin_keramik_config_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN) -module
kwin_keramik_config_la_LIBADD  = $(LIB_KDEUI)

METASOURCES                    = AUTO
noinst_HEADERS                 = config.h keramikconfig.h

lnkdir                         = $(kde_datadir)/kwin

