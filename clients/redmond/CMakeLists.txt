
include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )



########### next target ###############

set(kwin3_redmond_PART_SRCS redmond.cpp)

kde4_automoc(${kwin3_redmond_PART_SRCS})

kde4_add_plugin(kwin3_redmond ${kwin3_redmond_PART_SRCS})

kde4_install_libtool_file(${PLUGIN_INSTALL_DIR} kwin3_redmond)

target_link_libraries(kwin3_redmond ${KDE4_KDEFX_LIBS} kdecorations)

install(TARGETS kwin3_redmond  DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES redmond.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )


