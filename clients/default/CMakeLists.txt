
add_subdirectory( config ) 

include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )

########### next target ###############

set(kwin3_default_PART_SRCS kdedefault.cpp)

kde4_automoc(${kwin3_default_PART_SRCS})

kde4_add_plugin(kwin3_default ${kwin3_default_PART_SRCS})


target_link_libraries(kwin3_default ${KDE4_KDEFX_LIBS} kdecorations ${QT_QTGUI_LIBRARY})

install(TARGETS kwin3_default  DESTINATION ${PLUGIN_INSTALL_DIR})
