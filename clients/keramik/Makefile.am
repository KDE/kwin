INCLUDES                  = -I$(srcdir)/../../lib $(all_includes)

SUBDIRS                   = . config

noinst_PROGRAMS           = embedtool

noinst_HEADERS            = tiles.h

embedtool_SOURCES         = embedtool.cpp
embedtool_LDADD           = $(LIB_QT)
embedtool_LDFLAGS         = $(all_libraries) $(KDE_RPATH)

kde_module_LTLIBRARIES    = kwin3_keramik.la

kwin3_keramik_la_SOURCES   = keramik.cpp
kwin3_keramik_la_COMPILE_FIRST = tiles.h
kwin3_keramik_la_LDFLAGS   = $(all_libraries) $(KDE_PLUGIN) -module
kwin3_keramik_la_LIBADD    = $(LIB_KDEUI) ../../lib/libkdecorations.la
#kwin3_keramik_la_LDFLAGS   = $(all_libraries) -avoid-version -module $(KDE_RPATH) $(KDE_MT_LDFLAGS)

METASOURCES               = AUTO
noinst_headers        	  = keramik.h tiles.h

lnkdir                    = $(kde_datadir)/kwin
lnk_DATA                  = keramik.desktop

EXTRA_DIST                = $(lnk_DATA)

tiles.h: pics/caption-large-left.png pics/caption-small-right.png pics/titlebar-center.png \
	pics/titlebutton-square.png pics/border-left.png pics/caption-large-right.png \
	pics/grabbar-center.png pics/titlebar-left.png pics/border-right.png \
	pics/caption-small-center.png pics/grabbar-left.png pics/titlebar-right.png \
	pics/caption-large-center.png  pics/caption-small-left.png pics/grabbar-right.png \
	pics/titlebutton-round.png pics/bottom-left.png pics/bottom-right.png \
	pics/bottom-center.png \
	pics/titlebutton-square-large.png pics/titlebutton-square-huge.png \
	pics/titlebutton-round-large.png pics/titlebutton-round-huge.png

tiles.h: embedtool
	pics=`ls $(srcdir)/pics/*.png 2>/dev/null` ;\
	./embedtool $$pics

keramik.lo: tiles.h

