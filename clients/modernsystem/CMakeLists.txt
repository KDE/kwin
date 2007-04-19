
add_subdirectory( config ) 

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )


########### next target ###############

set(kwin3_modernsys_PART_SRCS modernsys.cpp )

kde4_automoc(kwin3_modernsys ${kwin3_modernsys_PART_SRCS})

kde4_add_plugin(kwin3_modernsys ${kwin3_modernsys_PART_SRCS})



target_link_libraries(kwin3_modernsys  ${KDE4_KDECORE_LIBS} kdecorations kdefx ${QT_QTGUI_LIBRARY})

install(TARGETS kwin3_modernsys  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES modernsystem.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

