



########### next target ###############

set(kwin3_web_PART_SRCS Web.cpp WebButton.cpp )

kde4_automoc(${kwin3_web_PART_SRCS})

kde4_add_plugin(kwin3_web ${kwin3_web_PART_SRCS})


target_link_libraries(kwin3_web  ${KDE4_KDEUI_LIBS} kdecorations )

install(TARGETS kwin3_web  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES web.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

