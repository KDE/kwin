



########### next target ###############

set(kwin3_mid_PART_SRCS mid.cpp)


kde4_add_plugin(kwin3_mid ${kwin3_mid_PART_SRCS})



target_link_libraries(kwin3_mid  ${KDE4_KDEUI_LIBS} kdecorations )

install(TARGETS kwin3_mid  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES mid.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

