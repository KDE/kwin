METASOURCES = AUTO
AM_CPPFLAGS = $(all_includes) -DKCMRULES
INCLUDES = -I$(top_srcdir)/kwin/lib

kde_module_LTLIBRARIES = kcm_kwinrules.la 

kcm_kwinrules_la_SOURCES = main.cpp ruleswidget.cpp ruleslist.cpp kwinsrc.cpp detectwidget.cpp \
        ruleswidgetbase.ui ruleslistbase.ui detectwidgetbase.ui
kcm_kwinrules_la_LDFLAGS = -module -avoid-version $(all_libraries) -no-undefined
kcm_kwinrules_la_LIBADD = $(LIB_KDEUI)

messages:
	$(XGETTEXT) *.cpp -o $(podir)/kcmkwm.pot

xdg_apps_DATA = kwinrules.desktop
