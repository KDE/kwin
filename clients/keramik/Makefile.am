INCLUDES                  = -I$(srcdir)/../.. $(all_includes)

SUBDIRS                   = . config

KDE_CXXFLAGS              = -UQT_NO_ASCII_CAST

kde_module_LTLIBRARIES    = kwin_keramik.la

kwin_keramik_la_SOURCES   = keramik.cpp
kwin_keramik_la_LIBADD    = ../../kwin.la
kwin_keramik_la_LDFLAGS   = $(all_libraries) $(KDE_PLUGIN) -module

METASOURCES               = AUTO
noinst_headers        	  = keramik.h tiles.h

lnkdir                    = $(kde_datadir)/kwin
lnk_DATA                  = keramik.desktop

EXTRA_DIST                = $(lnk_DATA)

QEMBED                    = $(QTDIR)/tools/qembed/qembed

keramik.lo: tiles.h

.PHONY: make-embed

make-embed: $(PICS)
	PICS=`ls $(srcdir)/pics/*.png 2>/dev/null` ;\
	$(QEMBED) --images $$pics | \
	sed -e 's,static QDict<QImage> dict;,,' | \
	sed -e 's,dict\.find,imageDict.find,' > $(srcdir)/tiles.h
