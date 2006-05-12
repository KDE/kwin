kde4_header()



########### install files ###############

install_files( ${DATA_INSTALL_DIR}/kwin/pics FILES close.png maximize.png maximizedown.png menu.png  	iconify.png pindown.png pinup.png unknown.png fog.png  	fog-grey.png bluesun.png greenie.light.png greenie.dim.png )

kde4_footer()



#original Makefile.am contents follow:

#
#kwin_pics_data_DATA = close.png maximize.png maximizedown.png menu.png \
#	iconify.png pindown.png pinup.png unknown.png fog.png \
#	fog-grey.png bluesun.png greenie.light.png greenie.dim.png
#
#kwin_pics_datadir = $(kde_datadir)/kwin/pics
#
#EXTRA_DIST = $(kwin_pics_data_DATA)
#
