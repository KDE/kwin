kde4_header()

add_subdirectory( decobenchmark ) 
include_directories( ${KDE4_INCLUDES} )


########### next target ###############

set(test_gravity_SRCS test_gravity.cpp )

kde4_automoc(${test_gravity_SRCS})

kde4_add_executable(test_gravity ${test_gravity_SRCS})

target_link_libraries(test_gravity  ${KDE4_KDECORE_LIBS} )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#noinst_PROGRAMS = test_gravity
#
#SUBDIRS = decobenchmark
#
#INCLUDES = $(all_includes)
#
#test_gravity_SOURCES = test_gravity.cpp
#test_gravity_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#test_gravity_LDADD = $(LIB_X11)
#
#METASOURCES = AUTO
