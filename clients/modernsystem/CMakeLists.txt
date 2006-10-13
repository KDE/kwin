
add_subdirectory( config ) 

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )


########### next target ###############

set(kwin3_modernsys_PART_SRCS modernsys.cpp)

kde4_automoc(kwin3_modernsys ${kwin3_modernsys_PART_SRCS})

kde4_add_plugin(kwin3_modernsys ${kwin3_modernsys_PART_SRCS})

kde4_install_libtool_file(${PLUGIN_INSTALL_DIR} kwin3_modernsys)

target_link_libraries(kwin3_modernsys ${KDE4_KDEFX_LIBS} kdecorations ${QT_QTGUI_LIBRARY})

install(TARGETS kwin3_modernsys  DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES modernsystem.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

