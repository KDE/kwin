add_definitions (-DQT3_SUPPORT)

add_subdirectory( config ) 

########### next target ###############

set(kwin3_keramik_PART_SRCS keramik.cpp )


qt4_add_resources(kwin3_keramik_PART_SRCS tiles.qrc )

kde4_add_plugin(kwin3_keramik ${kwin3_keramik_PART_SRCS})



target_link_libraries(kwin3_keramik  ${KDE4_KDEUI_LIBS} kdecorations ${QT_QT3SUPPORT_LIBRARY} ${QT_QTGUI_LIBRARY})

install(TARGETS kwin3_keramik  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES keramik.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )
