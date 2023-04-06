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
set_target_properties(kwin5_aurorae PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kdecoration2/"
    LIBRARY_OUTPUT_NAME "org.kde.kwin.aurorae"
)

target_compile_definitions(kwin5_aurorae PRIVATE
    -DTRANSLATION_DOMAIN=\"kwin\"
)

target_link_libraries(kwin5_aurorae
    KDecoration2::KDecoration
    kwineffects
    KF6::KCMUtils
    KF6::I18n
    KF6::Package
    KF6::WindowSystem
    Qt::Quick
    Qt::UiTools
)

install(TARGETS kwin5_aurorae DESTINATION ${KDE_INSTALL_PLUGINDIR}/org.kde.kdecoration2)

set(decoration_plugin_SRCS
    colorhelper.cpp
    decorationoptions.cpp
    decorationplugin.cpp
)

add_library(decorationplugin SHARED ${decoration_plugin_SRCS})
set_target_properties(decorationplugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org/kde/kwin/decoration/")
target_compile_definitions(decorationplugin PRIVATE
    -DTRANSLATION_DOMAIN=\"kwin\"
)
target_link_libraries(decorationplugin
    KDecoration2::KDecoration
    KF6::ConfigWidgets
    Qt::Quick
)
install(TARGETS decorationplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/decoration)

configure_file(aurorae.knsrc.cmake ${CMAKE_CURRENT_BINARY_DIR}/aurorae.knsrc)

########### install files ###############

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aurorae.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
install(
    FILES
        qml/AppMenuButton.qml
        qml/AuroraeButton.qml
        qml/AuroraeButtonGroup.qml
        qml/AuroraeMaximizeButton.qml
        qml/Decoration.qml
        qml/DecorationButton.qml
        qml/MenuButton.qml
        qml/aurorae.qml
    DESTINATION
        ${KDE_INSTALL_DATADIR}/kwin/aurorae)
set(QMLFILES
    qml/AppMenuButton.qml
    qml/ButtonGroup.qml
    qml/Decoration.qml
    qml/DecorationButton.qml
    qml/MenuButton.qml
    qml/qmldir
)
install(FILES ${QMLFILES} DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/kwin/decoration)
file(COPY ${QMLFILES} DESTINATION ${CMAKE_BINARY_DIR}/bin/org/kde/kwin/decoration/)
