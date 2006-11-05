
add_subdirectory( cli_installer ) 




########### next target ###############

set(kwin3_kwmtheme_PART_SRCS kwmthemeclient.cpp )

kde4_automoc(${kwin3_kwmtheme_PART_SRCS})

kde4_add_plugin(kwin3_kwmtheme ${kwin3_kwmtheme_PART_SRCS})


target_link_libraries(kwin3_kwmtheme  ${KDE4_KDECORE_LIBS} kdecorations )

install(TARGETS kwin3_kwmtheme  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES kwmtheme.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )


