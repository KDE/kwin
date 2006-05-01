INCLUDES = -I$(srcdir)/../../lib $(all_includes)

kde_module_LTLIBRARIES = kcm_kwindecoration.la

kcm_kwindecoration_la_SOURCES = kwindecoration.cpp buttons.cpp kwindecorationIface.skel preview.cpp
noinst_HEADERS = kwindecoration.h kwindecorationIface.h buttons.h preview.h

kcm_kwindecoration_la_LDFLAGS = \
  -module -avoid-version $(all_libraries) -no-undefined

kcm_kwindecoration_la_LIBADD = $(LIB_KDEUI) ../../lib/libkdecorations.la

METASOURCES = AUTO


xdg_apps_DATA = kwindecoration.desktop
