# $Id$
# kdebase/kcontrol/kwm

METASOURCES = AUTO
INCLUDES = $(all_includes)

kde_module_LTLIBRARIES = kcm_kwinoptions.la 

kcm_kwinoptions_la_SOURCES = windows.cpp mouse.cpp main.cpp
kcm_kwinoptions_la_LDFLAGS = -module -avoid-version $(all_libraries) -no-undefined
kcm_kwinoptions_la_LIBADD = $(LIB_KDEUI)

noinst_HEADERS = windows.h mouse.h 

messages:
	$(XGETTEXT) *.cpp -o $(podir)/kcmkwm.pot

kwin_kcmkwin_options_data_DATA = kwinactions.desktop  kwinadvanced.desktop  kwinfocus.desktop  kwinmoving.desktop  
kwin_kcmkwin_options_datadir = $(kde_appsdir)/.hidden

kwin_kcmkwin_options_data2_DATA = kwinoptions.desktop
kwin_kcmkwin_options_data2dir = $(kde_appsdir)/Settings/Desktop

