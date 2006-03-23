kde4_header()

add_subdirectory( kwinoptions ) 
add_subdirectory( kwindecoration ) 
add_subdirectory( kwinrules ) 
include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#SUBDIRS = kwinoptions kwindecoration kwinrules
