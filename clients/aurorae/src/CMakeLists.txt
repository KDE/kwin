########### decoration ###############
include_directories(
        ./lib
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(kwin3_aurorae_PART_SRCS
    aurorae.cpp
    lib/auroraetheme.cpp
    lib/themeconfig.cpp
)

kde4_add_plugin(kwin3_aurorae ${kwin3_aurorae_PART_SRCS})

target_link_libraries(kwin3_aurorae
    KF5::KConfigCore
    KF5::KService
    ${KDE4Support_LIBRARIES} # KGlobal
    ${KDE4_KDECORE_LIBS} # needed for KDebug
    ${Qt5Declarative_LIBRARIES}
    kdecorations
)

install(TARGETS kwin3_aurorae  DESTINATION ${PLUGIN_INSTALL_DIR} )

set(decoration_plugin_SRCS
    decorationplugin.cpp
    decorationoptions.cpp
    colorhelper.cpp
    )

add_library(decorationplugin SHARED ${decoration_plugin_SRCS})
target_link_libraries(decorationplugin
    KF5::KWidgets # KColorScheme
    ${KDE4_KDEUI_LIBRARY} # KGlobalSettings
    ${Qt5Declarative_LIBRARIES}
    kdecorations
)
install(TARGETS decorationplugin DESTINATION ${IMPORTS_INSTALL_DIR}/org/kde/kwin/decoration)

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
    qml/AppMenuButton.qml
    DESTINATION ${DATA_INSTALL_DIR}/kwin/aurorae )
install( FILES
    qml/Decoration.qml
    qml/DecorationButton.qml
    qml/MenuButton.qml
    qml/AppMenuButton.qml
    qml/ButtonGroup.qml
    qml/qmldir
    DESTINATION ${IMPORTS_INSTALL_DIR}/org/kde/kwin/decoration )
install( FILES kwindecoration.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )
