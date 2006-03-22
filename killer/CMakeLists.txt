kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp )

kde4_automoc(${kwin_killer_helper_SRCS})

kde4_add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})

target_link_libraries(kwin_killer_helper  ${KDE4_KDECORE_LIBS} kdeui )

install_targets(/bin kwin_killer_helper )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#INCLUDES = $(all_includes)
#
#bin_PROGRAMS = kwin_killer_helper
#
#kwin_killer_helper_SOURCES = killer.cpp
#kwin_killer_helper_LDADD = $(LIB_KDEUI)
#kwin_killer_helper_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#
#METASOURCES = AUTO
