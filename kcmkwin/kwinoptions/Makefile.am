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

data_DATA = kwinactions.desktop  kwinadvanced.desktop  kwinfocus.desktop  kwinmoving.desktop  
datadir = $(kde_appsdir)/.hidden

data2_DATA = kwinoptions.desktop
data2dir = $(kde_appsdir)/Settings/Desktop

