INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkcm_kwindecoration.la

libkcm_kwindecoration_la_SOURCES = kwindecoration.cpp buttons.cpp kwindecorationIface.skel
noinst_HEADERS = kwindecoration.h kwindecorationIface.h buttons.h

libkcm_kwindecoration_la_LDFLAGS = \
  -module -avoid-version $(all_libraries) -no-undefined

libkcm_kwindecoration_la_LIBADD = $(LIB_KDEUI)

METASOURCES = AUTO

messages:
	$(XGETTEXT) *.cpp -o $(podir)/kcmkwindecoration.pot

lnf_DATA = kwindecoration.desktop
lnfdir = $(kde_appsdir)/Settings/LookNFeel

EXTRA_DIST = $(lnf_DATA)
