


########### next target ###############

set(kdecorations_LIB_SRCS 
   kdecoration.cpp 
   kdecoration_p.cpp 
   kdecoration_plugins_p.cpp 
   kdecorationfactory.cpp 
   kcommondecoration.cpp )

kde4_automoc(kdecorations ${kdecorations_LIB_SRCS})

kde4_add_library(kdecorations SHARED ${kdecorations_LIB_SRCS})

target_link_libraries(kdecorations  ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY})

set_target_properties(kdecorations PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS kdecorations  DESTINATION ${LIB_INSTALL_DIR} )


########### install files ###############

install( FILES kdecoration.h kdecoration_p.h kdecoration_plugins_p.h kdecorationfactory.h kcommondecoration.h DESTINATION ${INCLUDE_INSTALL_DIR})
