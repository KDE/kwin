INCLUDES                  = -I$(srcdir)/../../lib $(all_includes)

kde_module_LTLIBRARIES    = kwin3_test.la

kwin3_test_la_SOURCES   = test.cpp
kwin3_test_la_LDFLAGS   = $(all_libraries) $(KDE_PLUGIN) -module
kwin3_test_la_LIBADD    = $(LIB_KDEUI) ../../lib/libkdecorations.la
#kwin_keramik_la_LDFLAGS   = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)

METASOURCES               = AUTO

kwin_test_lnkdir                    = $(kde_datadir)/kwin
kwin_test_lnk_DATA                  = test.desktop

EXTRA_DIST                = $(kwin_test_lnk_DATA)
