INCLUDES                  = -I$(top_srcdir) $(all_includes)
kde_module_LTLIBRARIES    = libkwinSuSE.la

libkwinSuSE_la_SOURCES     = \
  MachBunt.cpp \
  MachBuntButton.cpp \
  MachBuntButtonClose.cpp \
  MachBuntButtonMenu.cpp \
  MachBuntButtonHelp.cpp \
  MachBuntButtonIconify.cpp \
  MachBuntButtonLower.cpp \
  MachBuntButtonMaximize.cpp \
  MachBuntButtonSticky.cpp

noinst_HEADERS            = \
  MachBunt.h \
  MachBuntButton.h \
  MachBuntButtonClose.h \
  MachBuntButtonMenu.h \
  MachBuntButtonHelp.h \
  MachBuntButtonIconify.h \
  MachBuntButtonLower.h \
  MachBuntButtonMaximize.h \
  MachBuntButtonSticky.h

libkwinSuSE_la_LIBADD     = $(kde_libraries)/kwin.la
libkwinSuSE_la_LDFLAGS    = $(all_libraries) $(KDE_PLUGIN)
METASOURCES               = AUTO

linkdir                   = $(kde_datadir)/kwin/
link_DATA                 = SuSE.desktop
EXTRA_DIST                = $(link_DATA)
