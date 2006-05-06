kde4_header()
include_directories( ${KDE4_INCLUDES} )


########### next target ###############

set(kwmtheme_SRCS main.cpp )

kde4_automoc(${kwmtheme_SRCS})

kde4_add_executable(kwmtheme ${kwmtheme_SRCS})

target_link_libraries(kwmtheme  ${KDE4_KDECORE_LIBS} )

install(TARGETS kwmtheme DESTINATION bin)


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#
## set the include path for X, qt and KDE
#INCLUDES= $(all_includes)
#
######## This part is very kwmtheme specific
## you can add here more. This one gets installed 
#bin_PROGRAMS = 	kwmtheme
#
## Which sources should be compiled for kwmtheme.
#kwmtheme_SOURCES = main.cpp 
#
## the library search path. 
#kwmtheme_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#
## the libraries to link against. Be aware of the order. First the libraries,
## that depend on the following ones.
#kwmtheme_LDADD   = $(LIB_KDECORE)
#
