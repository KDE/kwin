METASOURCES = AUTO
AM_CPPFLAGS = $(all_includes) -DKCMRULES
INCLUDES = -I$(top_srcdir)/kwin/lib

bin_PROGRAMS =
lib_LTLIBRARIES =
kdeinit_LTLIBRARIES = kwin_rules_dialog.la
kde_module_LTLIBRARIES = kcm_kwinrules.la 
noinst_LTLIBRARIES = libkwinrules.la

libkwinrules_la_SOURCES = ruleswidget.cpp ruleslist.cpp kwinsrc.cpp detectwidget.cpp \
        ruleswidgetbase.ui ruleslistbase.ui detectwidgetbase.ui editshortcutbase.ui
libkwinrules_la_LDFLAGS = $(all_libraries) -no-undefined
libkwinrules_la_LIBADD = $(LIB_KDEUI)

kcm_kwinrules_la_SOURCES = kcm.cpp 
kcm_kwinrules_la_LDFLAGS = -module -avoid-version $(all_libraries) -no-undefined
kcm_kwinrules_la_LIBADD = libkwinrules.la 
kcm_kwinrules_la_COMPILE_FIRST = ruleslistbase.h ruleswidgetbase.h 

kwin_rules_dialog_la_SOURCES = main.cpp 
kwin_rules_dialog_la_LDFLAGS = -module -avoid-version $(all_libraries) -no-undefined
kwin_rules_dialog_la_LIBADD = libkwinrules.la 
kwin_rules_dialog_la_COMPILE_FIRST = ruleslistbase.h ruleswidgetbase.h

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kcmkwinrules.pot

xdg_apps_DATA = kwinrules.desktop
