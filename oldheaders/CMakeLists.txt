kde4_header()



########### install files ###############

install_files( /include/kwin FILES options.h client.h workspace.h kwinbutton.h )

kde4_footer()



#original Makefile.am contents follow:

#kwin_kwinincludedir = $(includedir)/kwin
#kwin_kwininclude_HEADERS = options.h client.h workspace.h kwinbutton.h
