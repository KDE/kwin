add_subdirectory( config )

set(kwin_tabstrip_SRCS
    tabstripbutton.cpp
    tabstripdecoration.cpp
    tabstripfactory.cpp
    )

kde4_add_plugin(kwin3_tabstrip ${kwin_tabstrip_SRCS})

target_link_libraries(kwin3_tabstrip kdecorations)

install(TARGETS kwin3_tabstrip  DESTINATION ${PLUGIN_INSTALL_DIR})

install( FILES tabstrip.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )
