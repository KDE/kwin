INCLUDES                  = -I$(top_srcdir) $(all_includes)
kde_module_LTLIBRARIES    = libkwinweb.la

libkwinweb_la_SOURCES     = \
  Web.cpp \
  WebButton.cpp \
  WebButtonClose.cpp \
  WebButtonHelp.cpp \
  WebButtonIconify.cpp \
  WebButtonLower.cpp \
  WebButtonMaximize.cpp \
  WebButtonSticky.cpp

noinst_HEADERS            = \
  Web.h \
  WebButton.h \
  WebButtonClose.h \
  WebButtonHelp.h \
  WebButtonIconify.h \
  WebButtonLower.h \
  WebButtonMaximize.h \
  WebButtonSticky.h

libkwinweb_la_LIBADD      = ../../kwin.la
libkwinweb_la_LDFLAGS     = $(all_libraries) $(KDE_PLUGIN)
METASOURCES               = AUTO

linkdir                   = $(kde_datadir)/kwin/
link_DATA                 = web.desktop
EXTRA_DIST                = $(link_DATA)
