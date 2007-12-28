
add_subdirectory( config )

########### next target ###############

set(kwin3_plastik_PART_SRCS
   plastik.cpp
   plastikclient.cpp
   plastikbutton.cpp
   )


kde4_add_plugin(kwin3_plastik ${kwin3_plastik_PART_SRCS})



target_link_libraries(kwin3_plastik kdecorations )

install(TARGETS kwin3_plastik  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES plastik.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

