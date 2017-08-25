########### decoration ###############
include_directories(
        ./lib
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(kwin5_aurorae_PART_SRCS
    aurorae.cpp
    decorationoptions.cpp
    lib/auroraetheme.cpp
    lib/themeconfig.cpp
)

add_library(kwin5_aurorae MODULE ${kwin5_aurorae_PART_SRCS})
set_target_properties(kwin5_aurorae PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kdecoration2/")

target_link_libraries(kwin5_aurorae
    KDecoration2::KDecoration
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Package
    KF5::WindowSystem
    Qt5::Quick
    Qt5::UiTools
)

install(TARGETS kwin5_aurorae DESTINATION ${PLUGIN_INSTALL_DIR}/org.kde.kdecoration2)

set(decoration_plugin_SRCS
    decorationplugin.cpp
    decorationoptions.cpp
    colorhelper.cpp
    )

add_library(decorationplugin SHARED ${decoration_plugin_SRCS})
target_link_libraries(decorationplugin
    Qt5::Quick
    KDecoration2::KDecoration
    KF5::ConfigWidgets
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
