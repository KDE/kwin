INCLUDES                  = -I$(top_srcdir) $(all_includes)
kde_module_LTLIBRARIES    = kwin3_web.la

kwin3_web_la_SOURCES      = Web.cpp WebButton.cpp

noinst_HEADERS            = Web.h WebButton.h

kwin3_web_la_LDFLAGS      = $(all_libraries) $(KDE_PLUGIN) -module
kwin3_web_la_LIBADD       = $(LIB_KDEUI) ../../lib/libkdecorations.la
METASOURCES               = AUTO

linkdir                   = $(kde_datadir)/kwin/
link_DATA                 = web.desktop
EXTRA_DIST                = $(link_DATA)

