
include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )


########### next target ###############

set(kwin3_laptop_PART_SRCS laptopclient.cpp)

kde4_automoc(kwin3_laptop ${kwin3_laptop_PART_SRCS})

kde4_add_plugin(kwin3_laptop ${kwin3_laptop_PART_SRCS})

kde4_install_libtool_file(${PLUGIN_INSTALL_DIR} kwin3_laptop)

target_link_libraries(kwin3_laptop ${KDE4_KDEFX_LIBS} kdecorations)

install(TARGETS kwin3_laptop  DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES laptop.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )

