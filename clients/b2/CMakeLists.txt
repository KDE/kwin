
add_subdirectory( config ) 

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib )


########### next target ###############

set(kwin3_b2_PART_SRCS b2client.cpp)

kde4_automoc(${kwin3_b2_PART_SRCS})

kde4_add_plugin(kwin3_b2 ${kwin3_b2_PART_SRCS})

target_link_libraries(kwin3_b2 kdecorations ${QT_QT3SUPPORT_LIBRARY})

install(TARGETS kwin3_b2  DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES b2.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )
