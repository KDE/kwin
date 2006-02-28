kde4_header()

add_subdirectory( plastik ) 
add_subdirectory( b2 ) 
add_subdirectory( default ) 
add_subdirectory( keramik ) 
add_subdirectory( laptop ) 
add_subdirectory( modernsystem ) 
add_subdirectory( quartz ) 
add_subdirectory( redmond ) 
add_subdirectory( web ) 

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#SUBDIRS = plastik b2 default keramik laptop modernsystem quartz redmond web
## need to be ported: kwmtheme (almost done)
## only for testing: test
#
#messages: rc.cpp
#	$(EXTRACTRC) `find . -name \*.ui` >> rc.cpp
#	$(XGETTEXT) `find . -name \*.cpp` -o $(podir)/kwin_clients.pot
#	-rm rc.cpp
