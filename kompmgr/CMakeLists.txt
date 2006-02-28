kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(kompmgr_SRCS kompmgr.c )

kde4_automoc(${kompmgr_SRCS})

kde4_add_executable(kompmgr ${kompmgr_SRCS})

target_link_libraries(kompmgr  ${QT_AND_KDECORE_LIBS} Xcomposite Xdamage Xfixes Xext m )

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
