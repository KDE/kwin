
add_subdirectory( config )

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )


########### next target ###############

set(kwin3_plastik_PART_SRCS 
   plastik.cpp 
   plastikclient.cpp 
   plastikbutton.cpp 
   misc.cpp )

kde4_automoc(kwin3_plastik ${kwin3_plastik_PART_SRCS})

kde4_add_plugin(kwin3_plastik ${kwin3_plastik_PART_SRCS})



target_link_libraries(kwin3_plastik  ${KDE4_KDEUI_LIBS} kdecorations kdefx)

install(TARGETS kwin3_plastik  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES plastik.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

