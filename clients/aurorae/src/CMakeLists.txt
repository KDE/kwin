########### decoration ###############
include_directories( ./lib )

set(kwin3_aurorae_PART_SRCS
    aurorae.cpp
    lib/auroraebutton.cpp
    lib/auroraescene.cpp
    lib/auroraetab.cpp
    lib/auroraetheme.cpp
    lib/themeconfig.cpp
)

kde4_add_plugin(kwin3_aurorae ${kwin3_aurorae_PART_SRCS})

target_link_libraries(kwin3_aurorae ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} kdecorations)

install(TARGETS kwin3_aurorae  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES aurorae.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )
install( FILES aurorae.knsrc  DESTINATION  ${CONFIG_INSTALL_DIR} )
