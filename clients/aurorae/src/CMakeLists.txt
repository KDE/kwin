########### decoration ###############
include_directories( ./lib )

set(kwin3_aurorae_PART_SRCS
    aurorae.cpp
    lib/auroraetheme.cpp
    lib/themeconfig.cpp
)

kde4_add_plugin(kwin3_aurorae ${kwin3_aurorae_PART_SRCS})

target_link_libraries(kwin3_aurorae ${KDE4_KDEUI_LIBS} ${QT_QTDECLARATIVE_LIBRARY} kdecorations)

install(TARGETS kwin3_aurorae  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES aurorae.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin )
install( FILES aurorae.knsrc  DESTINATION  ${CONFIG_INSTALL_DIR} )
install( FILES
    qml/aurorae.qml
    qml/AuroraeButton.qml
    qml/AuroraeButtonGroup.qml
    qml/AuroraeMaximizeButton.qml
    qml/Decoration.qml
    qml/DecorationButton.qml
    qml/MenuButton.qml
    DESTINATION ${DATA_INSTALL_DIR}/kwin/aurorae )
install( FILES kwindecoration.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )
