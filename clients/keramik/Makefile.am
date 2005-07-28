INCLUDES                  = -I$(srcdir)/../../lib $(all_includes)

SUBDIRS                   = . config

kde_module_LTLIBRARIES    = kwin3_keramik.la

kwin3_keramik_la_SOURCES   = keramik.cpp tiles.qrc
kwin3_keramik_la_LDFLAGS   = $(all_libraries) $(KDE_PLUGIN) -module
kwin3_keramik_la_LIBADD    = $(LIB_KDEUI) ../../lib/libkdecorations.la
#kwin3_keramik_la_LDFLAGS   = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)

METASOURCES               = AUTO
noinst_headers        	  = keramik.h

lnkdir                    = $(kde_datadir)/kwin
lnk_DATA                  = keramik.desktop

EXTRA_DIST                = $(lnk_DATA)


