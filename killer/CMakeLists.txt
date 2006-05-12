


########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp )

kde4_automoc(${kwin_killer_helper_SRCS})

kde4_add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})

target_link_libraries(kwin_killer_helper  ${KDE4_KDEUI_LIBS} )

install(TARGETS kwin_killer_helper DESTINATION bin)


########### install files ###############





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
