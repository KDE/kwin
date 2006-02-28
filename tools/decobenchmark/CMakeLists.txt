kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(decobenchmark_SRCS main.cpp preview.cpp )

kde4_automoc(${decobenchmark_SRCS})

kde4_add_executable(decobenchmark ${decobenchmark_SRCS})

target_link_libraries(decobenchmark  ${QT_AND_KDECORE_LIBS} kdecorations )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#noinst_PROGRAMS = decobenchmark
#
#INCLUDES = $(all_includes)
#
#decobenchmark_SOURCES = main.cpp preview.cpp
#decobenchmark_LDFLAGS = $(all_libraries)
#decobenchmark_LDADD = ../../lib/libkdecorations.la
#
#METASOURCES = AUTO
