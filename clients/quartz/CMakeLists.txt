
add_subdirectory( config ) 




########### next target ###############

set(kwin3_quartz_PART_SRCS quartz.cpp )

kde4_automoc(kwin3_quartz ${kwin3_quartz_PART_SRCS})

kde4_add_plugin(kwin3_quartz ${kwin3_quartz_PART_SRCS})



target_link_libraries(kwin3_quartz  ${KDE4_KDEUI_LIBS} kdecorations kdefx)

install(TARGETS kwin3_quartz  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES quartz.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )


