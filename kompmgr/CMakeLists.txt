kde4_header()
include_directories( ${KDE4_INCLUDES} )


########### next target ###############

set(kompmgr_SRCS kompmgr.c )

kde4_automoc(${kompmgr_SRCS})

kde4_add_executable(kompmgr ${kompmgr_SRCS})

target_link_libraries(kompmgr  ${KDE4_KDECORE_LIBS} ${X11_Xcomposite_LIB} ${X11_Xdamage_LIB} ${X11_Xfixes_LIB} ${X11_Xext_LIB} m )

install_targets(/bin kompmgr )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#bin_PROGRAMS = kompmgr
#
#kompmgr_SOURCES = kompmgr.c
#kompmgr_LDADD =  $(X_LDFLAGS) $(X_RPATH) -lXcomposite -lXdamage -lXfixes $(LIB_XRENDER) $(LIB_X11) -lXext -lm
#
#INCLUDES = $(all_includes)
