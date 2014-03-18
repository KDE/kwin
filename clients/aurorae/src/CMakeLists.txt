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

add_library(kwin3_aurorae MODULE ${kwin3_aurorae_PART_SRCS})

target_link_libraries(kwin3_aurorae
    KF5::ConfigCore
    KF5::Service
    Qt5::Quick
    Qt5::Widgets
    kdecorations
)

kservice_desktop_to_json(kwin3_aurorae aurorae.desktop)

install(TARGETS kwin3_aurorae  DESTINATION ${PLUGIN_INSTALL_DIR}/kwin/kdecorations )

set(decoration_plugin_SRCS
    decorationplugin.cpp
    decorationoptions.cpp
    colorhelper.cpp
    )

add_library(decorationplugin SHARED ${decoration_plugin_SRCS})
target_link_libraries(decorationplugin
    Qt5::Quick
    kdecorations
    KF5::ConfigWidgets
    KF5::Plasma
)
install(TARGETS decorationplugin DESTINATION ${QML_INSTALL_DIR}/org/kde/kwin/decoration)

########### install files ###############

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
    DESTINATION ${QML_INSTALL_DIR}/org/kde/kwin/decoration )
install( FILES kwindecoration.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR} )
