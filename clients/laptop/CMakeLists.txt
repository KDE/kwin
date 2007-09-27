
########### next target ###############

set(kwin3_laptop_PART_SRCS laptopclient.cpp)


kde4_add_plugin(kwin3_laptop ${kwin3_laptop_PART_SRCS})

target_link_libraries(kwin3_laptop kdecorations)

install(TARGETS kwin3_laptop  DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES laptop.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

