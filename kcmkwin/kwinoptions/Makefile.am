# $Id$
# kdebase/kcontrol/kwm

METASOURCES = AUTO
INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = libkcm_kwinoptions.la 

libkcm_kwinoptions_la_SOURCES = windows.cpp mouse.cpp main.cpp
libkcm_kwinoptions_la_LDFLAGS = -module -avoid-version $(all_libraries) -no-undefined
libkcm_kwinoptions_la_LIBADD = $(LIB_KDEUI)

noinst_HEADERS = windows.h mouse.h main.h

messages:
	$(XGETTEXT) *.cpp -o $(podir)/kcmkwm.pot

data_DATA = kwinoptions.desktop  
datadir = $(kde_appsdir)/Settings/LookNFeel

install-data-local: uninstall.desktop
	$(mkinstalldirs) $(kde_appsdir)/Settings/LookNFeel/Windows
	$(INSTALL_DATA) $(srcdir)/uninstall.desktop $(kde_appsdir)/Settings/LookNFeel/Windows/actions.desktop
	$(INSTALL_DATA) $(srcdir)/uninstall.desktop $(kde_appsdir)/Settings/LookNFeel/Windows/kwinmouse.desktop
	$(INSTALL_DATA) $(srcdir)/uninstall.desktop $(kde_appsdir)/Settings/LookNFeel/Windows/mouse.desktop
