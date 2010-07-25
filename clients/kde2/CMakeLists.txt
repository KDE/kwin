
add_subdirectory( config ) 

########### next target ###############

set(kwin3_kde2_PART_SRCS kde2.cpp)


kde4_add_plugin(kwin3_kde2 ${kwin3_kde2_PART_SRCS})

target_link_libraries(kwin3_kde2 kdecorations ${QT_QTGUI_LIBRARY})

install(TARGETS kwin3_kde2  DESTINATION ${PLUGIN_INSTALL_DIR})

install( FILES kde2.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

