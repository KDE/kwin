



########### next target ###############

set(kwin3_test_PART_SRCS test.cpp )

kde4_automoc(${kwin3_test_PART_SRCS})

kde4_add_plugin(kwin3_test ${kwin3_test_PART_SRCS})


target_link_libraries(kwin3_test  ${KDE4_KDEUI_LIBS} kdecorations )

install(TARGETS kwin3_test  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES test.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

